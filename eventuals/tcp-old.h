#pragma once

#include "eventuals/event-loop.h"

#if !defined(_WIN32)
#include "asio/ssl.hpp"
#endif

////////////////////////////////////////////////////////////////////////

namespace eventuals {
namespace ip {
namespace tcp {

////////////////////////////////////////////////////////////////////////

enum class Protocol {
  UNKNOWN,
  IPV4,
  IPV6,
};

////////////////////////////////////////////////////////////////////////

enum class ShutdownType {
  SEND,
  RECEIVE,
  BOTH,
};

////////////////////////////////////////////////////////////////////////

class Socket {
 public:
  Socket() = delete;
  Socket(EventLoop& loop)
    : socket_(loop.io_context()) {}

  Socket(const Socket& that) = delete;
  Socket(Socket&& that) = default;

  Socket& operator=(const Socket& that) = delete;
  Socket& operator=(Socket&& that) = default;

  auto Open(Protocol protocol) {
    return RescheduleAfter(_Open::Composable{*this, protocol});
  }

  auto Bind(const std::string& ip, uint16_t port) {
    return RescheduleAfter(_Bind::Composable{*this, ip, port});
  }

  auto Connect(const std::string& ip, uint16_t port) {
    return RescheduleAfter(_Connect::Composable{*this, ip, port});
  }

  auto ReceiveSome(
      void* destination,
      size_t destination_size) {
    return RescheduleAfter(
        _Receive::Composable{
            *this,
            destination,
            destination_size});
  }

  auto Receive(
      void* destination,
      size_t destination_size,
      size_t bytes_to_read) {
    return RescheduleAfter(
        _ReceiveNBytes::Composable{
            *this,
            destination,
            destination_size,
            bytes_to_read});
  }

  auto Send(const void* source, size_t source_size) {
    return RescheduleAfter(_Send::Composable{*this, source, source_size});
  }

  auto Shutdown(ShutdownType shutdown_type) {
    return RescheduleAfter(_Shutdown::Composable{*this, shutdown_type});
  }

  auto Close() {
    return RescheduleAfter(_Close::Composable{*this});
  }

  bool IsOpen() {
    return underlying_handle().is_open();
  }

  uint16_t BindedPort() {
    return underlying_handle().local_endpoint().port();
  }

  std::string BindedIp() {
    return underlying_handle().local_endpoint().address().to_string();
  }

 private:
  asio::ip::tcp::socket socket_;
  Protocol protocol_ = Protocol::UNKNOWN;

  asio::ip::tcp::socket& underlying_handle() {
    return socket_;
  }

  friend class Acceptor;

  struct _Open {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Socket& socket, Protocol protocol)
        : k_(std::move(k)),
          socket_(socket),
          protocol_(protocol) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          protocol_(that.protocol_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        switch (protocol_) {
          case Protocol::IPV4:
            socket_.underlying_handle().open(asio::ip::tcp::v4(), ec);
            break;
          case Protocol::IPV6:
            socket_.underlying_handle().open(asio::ip::tcp::v6(), ec);
            break;
          default:
            k_.Fail("Unknown protocol");
            return;
        }

        socket_.protocol_ = protocol_;

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      Protocol protocol_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_, protocol_};
      }

      Socket& socket_;
      Protocol protocol_ = Protocol::UNKNOWN;
    };
  };

  struct _Bind {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Socket& socket, std::string&& ip, uint16_t port)
        : k_(std::move(k)),
          socket_(socket),
          ip_(std::move(ip)),
          port_(port) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          ip_(std::move(that.ip_)),
          port_(that.port_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;
        asio::ip::tcp::endpoint endpoint;

        switch (socket_.protocol_) {
          case Protocol::IPV4:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v4(ip_, ec),
                port_);
            break;
          case Protocol::IPV6:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v6(ip_, ec),
                port_);
            break;
          default:
            k_.Fail("Unknown protocol");
            return;
        }

        if (ec) {
          k_.Fail(ec.message());
          return;
        }

        socket_.underlying_handle().bind(endpoint, ec);

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      std::string ip_;
      uint16_t port_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_, std::move(ip_), port_};
      }

      Socket& socket_;
      std::string ip_;
      uint16_t port_;
    };
  };

  struct _Connect {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Socket& socket, std::string&& ip, uint16_t port)
        : k_(std::move(k)),
          socket_(socket),
          ip_(std::move(ip)),
          port_(port) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          ip_(std::move(that.ip_)),
          port_(that.port_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;

        asio::error_code ec;
        asio::ip::tcp::endpoint endpoint;

        switch (socket_.protocol_) {
          case Protocol::IPV4:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v4(ip_, ec),
                port_);
            break;
          case Protocol::IPV6:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v6(ip_, ec),
                port_);
            break;
          default:
            k_.Fail("Unknown protocol");
            return;
        }

        if (ec) {
          k_.Fail(ec.message());
          return;
        }

        socket_.underlying_handle().async_connect(
            endpoint,
            [this](const asio::error_code& ec) {
              completed_ = true;

              if (!ec) {
                k_.Start();
              } else {
                k_.Fail(ec.message());
              }
            });
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      std::string ip_;
      uint16_t port_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_, std::move(ip_), port_};
      }

      Socket& socket_;
      std::string ip_;
      uint16_t port_;
    };
  };

  struct _Receive {
    template <typename K_>
    struct Continuation {
      Continuation(
          K_ k,
          Socket& socket,
          void* destination,
          size_t destination_size)
        : k_(std::move(k)),
          socket_(socket),
          destination_(destination),
          destination_size_(destination_size) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          destination_(that.destination_),
          destination_size_(that.destination_size_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;

        socket_.underlying_handle().async_read_some(
            asio::buffer(destination_, destination_size_),
            [this](const asio::error_code& ec, std::size_t bytes_transferred) {
              completed_ = true;

              if (!ec) {
                k_.Start(bytes_transferred);
              } else {
                k_.Fail(ec.message());
              }
            });
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      void* destination_;
      size_t destination_size_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = size_t;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{
            std::move(k),
            socket_,
            destination_,
            destination_size_};
      }

      Socket& socket_;
      void* destination_;
      size_t destination_size_;
    };
  };

  struct _ReceiveNBytes {
    template <typename K_>
    struct Continuation {
      Continuation(
          K_ k,
          Socket& socket,
          void* destination,
          size_t destination_size,
          size_t bytes_to_read)
        : k_(std::move(k)),
          socket_(socket),
          destination_(destination),
          destination_size_(destination_size),
          bytes_to_read_(std::min(bytes_to_read, destination_size)) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          destination_(that.destination_),
          destination_size_(that.destination_size_),
          bytes_to_read_(that.bytes_to_read_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;

        if (bytes_to_read_ == 0) {
          completed_ = true;
          k_.Start(0);
          return;
        }

        // Start receiving.
        asio::async_read(
            socket_.underlying_handle(),
            asio::buffer(
                destination_,
                destination_size_),
            [this](const asio::error_code& ec, size_t bytes_transferred) {
              completed_ = true;
              if (!ec) {
                k_.Start(bytes_transferred);
              } else {
                k_.Fail(ec.message());
              }
            });
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      void* destination_;
      size_t destination_size_;
      size_t bytes_to_read_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = size_t;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{
            std::move(k),
            socket_,
            destination_,
            destination_size_,
            bytes_to_read_};
      }

      Socket& socket_;
      void* destination_;
      size_t destination_size_;
      size_t bytes_to_read_;
    };
  };

  struct _Send {
    template <typename K_>
    struct Continuation {
      Continuation(
          K_ k,
          Socket& socket,
          const void* source,
          size_t source_size)
        : k_(std::move(k)),
          socket_(socket),
          source_(source),
          source_size_(source_size) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          source_(that.source_),
          source_size_(that.source_size_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;

        if (source_size_ == 0) {
          completed_ = true;
          k_.Start(0);
          return;
        }

        // Will only succeed after writing all of the data to socket.
        asio::async_write(
            socket_.underlying_handle(),
            asio::buffer(source_, source_size_),
            [this](const asio::error_code& ec, size_t bytes_transferred) {
              completed_ = true;
              if (!ec) {
                k_.Start(bytes_transferred);
              } else {
                k_.Fail(ec.message());
              }
            });
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      const void* source_;
      size_t source_size_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = size_t;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_, source_, source_size_};
      }

      Socket& socket_;
      const void* source_;
      size_t source_size_;
    };
  };

  struct _Shutdown {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Socket& socket, ShutdownType shutdown_type)
        : k_(std::move(k)),
          socket_(socket),
          shutdown_type_(shutdown_type) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_),
          shutdown_type_(that.shutdown_type_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        switch (shutdown_type_) {
          case ShutdownType::BOTH:
            socket_.underlying_handle().shutdown(
                asio::ip::tcp::socket::shutdown_both,
                ec);
            break;
          case ShutdownType::RECEIVE:
            socket_.underlying_handle().shutdown(
                asio::ip::tcp::socket::shutdown_receive,
                ec);
            break;
          case ShutdownType::SEND:
            socket_.underlying_handle().shutdown(
                asio::ip::tcp::socket::shutdown_send,
                ec);
            break;
          default:
            k_.Fail("Unknown shutdown type");
            return;
        }

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;
      ShutdownType shutdown_type_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_, shutdown_type_};
      }

      Socket& socket_;
      ShutdownType shutdown_type_;
    };
  };

  struct _Close {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Socket& socket)
        : k_(std::move(k)),
          socket_(socket) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          socket_(that.socket_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        socket_.underlying_handle().close(ec);

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Socket& socket_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), socket_};
      }

      Socket& socket_;
    };
  };
};

////////////////////////////////////////////////////////////////////////

// BoringSSL is required for SSL Sockets to work.
#if !defined(_WIN32)

namespace ssl {

////////////////////////////////////////////////////////////////////////

enum class SSLVersion {

};

////////////////////////////////////////////////////////////////////////

class Socket {
};

////////////////////////////////////////////////////////////////////////

} // namespace ssl

#endif

////////////////////////////////////////////////////////////////////////

class Acceptor {
 public:
  Acceptor() = delete;
  Acceptor(EventLoop& loop)
    : acceptor_(loop.io_context()) {}

  Acceptor(const Acceptor& that) = delete;
  Acceptor(Acceptor&& that) = default;

  Acceptor& operator=(const Acceptor& that) = delete;
  Acceptor& operator=(Acceptor&& that) = default;

  auto Open(Protocol protocol) {
    return RescheduleAfter(_Open::Composable{*this, protocol});
  }

  auto Bind(const std::string& ip, uint16_t port) {
    return RescheduleAfter(_Bind::Composable{*this, ip, port});
  }

  auto Listen() {
    return RescheduleAfter(_Listen::Composable{*this});
  }

  auto Accept(Socket& to_socket) {
    return RescheduleAfter(_Accept::Composable<Socket>{*this, to_socket});
  }

#if !defined(_WIN32)
  auto Accept(ssl::Socket& to_socket) {
    return RescheduleAfter(_Accept::Composable<ssl::Socket>{*this, to_socket});
  }
#endif

  auto Close() {
    return RescheduleAfter(_Close::Composable{*this});
  }

  bool IsOpen() {
    return underlying_handle().is_open();
  }

  uint16_t BindedPort() {
    return underlying_handle().local_endpoint().port();
  }

  std::string BindedIp() {
    return underlying_handle().local_endpoint().address().to_string();
  }

 private:
  asio::ip::tcp::acceptor acceptor_;
  Protocol protocol_ = Protocol::UNKNOWN;

  asio::ip::tcp::acceptor& underlying_handle() {
    return acceptor_;
  }

  struct _Open {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Acceptor& acceptor, Protocol protocol)
        : k_(std::move(k)),
          acceptor_(acceptor),
          protocol_(protocol) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          acceptor_(that.acceptor_),
          protocol_(that.protocol_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        switch (protocol_) {
          case Protocol::IPV4:
            acceptor_.underlying_handle().open(asio::ip::tcp::v4(), ec);
            break;
          case Protocol::IPV6:
            acceptor_.underlying_handle().open(asio::ip::tcp::v6(), ec);
            break;
          default:
            k_.Fail("Unknown protocol");
            return;
        }

        acceptor_.protocol_ = protocol_;

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Acceptor& acceptor_;
      Protocol protocol_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), acceptor_, protocol_};
      }

      Acceptor& acceptor_;
      Protocol protocol_ = Protocol::UNKNOWN;
    };
  };

  struct _Bind {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Acceptor& acceptor, std::string&& ip, uint16_t port)
        : k_(std::move(k)),
          acceptor_(acceptor),
          ip_(std::move(ip)),
          port_(port) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          acceptor_(that.acceptor_),
          ip_(std::move(that.ip_)),
          port_(that.port_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;
        asio::ip::tcp::endpoint endpoint;

        switch (acceptor_.protocol_) {
          case Protocol::IPV4:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v4(ip_, ec),
                port_);
            break;
          case Protocol::IPV6:
            endpoint = asio::ip::tcp::endpoint(
                asio::ip::make_address_v6(ip_, ec),
                port_);
            break;
          default:
            k_.Fail("Unknown protocol");
            return;
        }

        if (ec) {
          k_.Fail(ec.message());
          return;
        }

        acceptor_.underlying_handle().bind(endpoint, ec);

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Acceptor& acceptor_;
      std::string ip_;
      uint16_t port_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), acceptor_, std::move(ip_), port_};
      }

      Acceptor& acceptor_;
      std::string ip_;
      uint16_t port_;
    };
  };

  struct _Listen {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Acceptor& acceptor)
        : k_(std::move(k)),
          acceptor_(acceptor) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          acceptor_(that.acceptor_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        acceptor_.underlying_handle().listen(
            asio::socket_base::max_listen_connections,
            ec);

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Acceptor& acceptor_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), acceptor_};
      }

      Acceptor& acceptor_;
    };
  };

  struct _Accept {
    template <typename K_, typename SocketType>
    struct Continuation {
      Continuation(K_ k, Acceptor& acceptor, SocketType& to_socket)
        : k_(std::move(k)),
          acceptor_(acceptor),
          to_socket_(to_socket) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          acceptor_(that.acceptor_),
          to_socket_(that.to_socket_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;

        acceptor_.underlying_handle().async_accept(
            to_socket_.underlying_handle(),
            [this](const asio::error_code& ec) {
              completed_ = true;

              if (!ec) {
                k_.Start();
              } else {
                k_.Fail(ec.message());
              }
            });
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Acceptor& acceptor_;
      SocketType& to_socket_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    template <typename SocketType>
    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K, SocketType>{std::move(k), acceptor_, to_socket_};
      }

      Acceptor& acceptor_;
      SocketType& to_socket_;
    };
  };

  struct _Close {
    template <typename K_>
    struct Continuation {
      Continuation(K_ k, Acceptor& acceptor)
        : k_(std::move(k)),
          acceptor_(acceptor) {}

      Continuation(Continuation&& that)
        : k_(std::move(that.k_)),
          acceptor_(that.acceptor_) {
        CHECK(!that.started_ || !that.completed_) << "moving after starting";
        CHECK(!handler_);
      }

      void Start() {
        CHECK(!started_ && !completed_);

        started_ = true;
        completed_ = true;

        asio::error_code ec;

        acceptor_.underlying_handle().close(ec);

        if (!ec) {
          k_.Start();
        } else {
          k_.Fail(ec.message());
        }
      }

      template <typename... Args>
      void Fail(Args&&... args) {
        k_.Fail(std::forward<Args>(args)...);
      }

      void Stop() {
        k_.Stop();
      }

      void Register(class Interrupt& interrupt) {
        k_.Register(interrupt);

        handler_.emplace(&interrupt, [this]() {
          if (!completed_) {
            completed_ = true;
            k_.Stop();
          }
        });

        // NOTE: we always install the handler in case 'Start()'
        // never gets called.
        handler_->Install();
      }

     private:
      K_ k_;

      Acceptor& acceptor_;

      bool started_ = false;
      bool completed_ = false;

      std::optional<Interrupt::Handler> handler_;
    };

    struct Composable {
      template <typename>
      using ValueFrom = void;

      template <typename Arg, typename K>
      auto k(K k) && {
        return Continuation<K>{std::move(k), acceptor_};
      }

      Acceptor& acceptor_;
    };
  };
};

////////////////////////////////////////////////////////////////////////

} // namespace tcp
} // namespace ip
} // namespace eventuals
