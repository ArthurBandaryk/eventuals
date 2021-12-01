#pragma once

#include "eventuals/event-loop.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {
namespace ip {
namespace tcp {

////////////////////////////////////////////////////////////////////////

enum class Protocol {
  NOT_SET,
  IPV4,
  IPV6,
};

////////////////////////////////////////////////////////////////////////

class Socket {
 public:
  Socket(EventLoop& loop = EventLoop::Default())
    : loop_(loop),
      socket_(loop.io_context()) {}

  Socket(const Socket& that) = delete;
  Socket(Socket&& that)
    : loop_(that.loop_),
      socket_(std::move(that.socket_)),
      protocol_(that.protocol_) {}

  Socket& operator=(const Socket& that) = delete;
  Socket& operator=(Socket&& that) = delete;

  auto Open(Protocol protocol) {
    struct Data {
      Socket* socket;
      Protocol protocol;
    };

    return Eventual<void>()
        .interruptible()
        .context(Data{this, protocol})
        .start([](auto& data, auto& k, Interrupt::Handler& handler) {
          asio::post(
              data.socket->io_context(),
              [data,
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;

                  switch (data.protocol) {
                    case Protocol::IPV4:
                      data.socket->underlying_handle().open(
                          asio::ip::tcp::v4(),
                          ec);
                      break;
                    case Protocol::IPV6:
                      data.socket->underlying_handle().open(
                          asio::ip::tcp::v6(),
                          ec);
                      break;
                    default:
                      k.Fail("Unknown protocol");
                      return;
                  }

                  data.socket->protocol_ = data.protocol;

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  auto Bind(std::string&& ip, uint16_t port) {
    struct Data {
      Socket* socket;
      std::string ip;
      uint16_t port;
    };

    return Eventual<void>()
        .interruptible()
        .context(Data{this, std::move(ip), port})
        .start([](auto& data, auto& k, Interrupt::Handler& handler) {
          asio::post(
              data.socket->io_context(),
              [data = std::move(data),
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;
                  asio::ip::tcp::endpoint endpoint;

                  switch (data.socket->protocol_) {
                    case Protocol::IPV4:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v4(data.ip, ec),
                          data.port);
                      break;
                    case Protocol::IPV6:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v6(data.ip, ec),
                          data.port);
                      break;
                    default:
                      k.Fail("Unknown protocol");
                      return;
                  }

                  if (ec) {
                    k.Fail(ec.message());
                    return;
                  }

                  data.socket->underlying_handle().bind(endpoint, ec);

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  auto Connect(std::string&& ip, uint16_t port) {
    struct Data {
      Socket* socket;
      std::string ip;
      uint16_t port;

      // Used for interrupt handler due to
      // static_assert(sizeof(Handler<F>) <= SIZE) (callback.h(59,5))
      // requirement for handler.Install().
      void* k = nullptr;

      bool started = false;
      bool completed = false;
    };

    return Eventual<void>()
        .interruptible()
        .context(Data{this, std::move(ip), port})
        .start([](auto& data, auto& k, Interrupt::Handler& handler) {
          using K = std::decay_t<decltype(k)>;
          data.k = &k;

          handler.Install([&data]() {
            asio::post(data.socket->io_context(), [&]() {
              K& k = *static_cast<K*>(data.k);

              if (!data.started) {
                data.completed = true;
                k.Stop();
              } else if (!data.completed) {
                data.completed = true;
                asio::error_code ec;
                data.socket->underlying_handle().cancel(ec);

                if (!ec) {
                  k.Stop();
                } else {
                  k.Fail(ec.message());
                }
              }
            });
          });

          asio::post(
              data.socket->io_context(),
              [&]() {
                if (!data.completed) {
                  CHECK(!data.started);
                  data.started = true;

                  asio::error_code ec;
                  asio::ip::tcp::endpoint endpoint;

                  switch (data.socket->protocol_) {
                    case Protocol::IPV4:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v4(data.ip, ec),
                          data.port);
                      break;
                    case Protocol::IPV6:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v6(data.ip, ec),
                          data.port);
                      break;
                    default:
                      data.completed = true;
                      k.Fail("Unknown protocol");
                      return;
                  }

                  if (ec) {
                    data.completed = true;
                    k.Fail(ec.message());
                  }

                  data.socket->underlying_handle().async_connect(
                      endpoint,
                      [&](const asio::error_code& ec) {
                        if (!data.completed) {
                          data.completed = true;

                          if (!ec) {
                            k.Start();
                          } else {
                            k.Fail(ec.message());
                          }
                        }
                      });
                }
              });
        });
  }

  auto Close() {
    return Eventual<void>()
        .interruptible()
        .context(this)
        .start([](Socket* socket, auto& k, Interrupt::Handler& handler) {
          asio::post(
              socket->io_context(),
              [socket,
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;

                  socket->underlying_handle().close(ec);

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  bool IsOpen() {
    return underlying_handle().is_open();
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  uint16_t BindedPort() {
    return underlying_handle().local_endpoint().port();
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  std::string BindedIp() {
    return underlying_handle().local_endpoint().address().to_string();
  }

 private:
  EventLoop& loop_;
  asio::ip::tcp::socket socket_;
  Protocol protocol_ = Protocol::NOT_SET;

  asio::ip::tcp::socket& underlying_handle() {
    return socket_;
  }

  asio::io_context& io_context() {
    return loop_.io_context();
  }
};

////////////////////////////////////////////////////////////////////////

class Acceptor {
 public:
  Acceptor(EventLoop& loop = EventLoop::Default())
    : loop_(loop),
      acceptor_(loop.io_context()) {}

  Acceptor(const Acceptor& that) = delete;
  Acceptor(Acceptor&& that)
    : loop_(that.loop_),
      acceptor_(std::move(that.acceptor_)),
      protocol_(that.protocol_) {}

  Acceptor& operator=(const Acceptor& that) = delete;
  Acceptor& operator=(Acceptor&& that) = delete;

  auto Open(Protocol protocol) {
    struct Data {
      Acceptor* acceptor;
      Protocol protocol;
    };

    return Eventual<void>()
        .interruptible()
        .context(Data{this, protocol})
        .start([](auto& data, auto& k, Interrupt::Handler& handler) {
          asio::post(
              data.acceptor->io_context(),
              [data,
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;

                  switch (data.protocol) {
                    case Protocol::IPV4:
                      data.acceptor->underlying_handle().open(
                          asio::ip::tcp::v4(),
                          ec);
                      break;
                    case Protocol::IPV6:
                      data.acceptor->underlying_handle().open(
                          asio::ip::tcp::v6(),
                          ec);
                      break;
                    default:
                      k.Fail("Unknown protocol");
                      return;
                  }

                  data.acceptor->protocol_ = data.protocol;

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  auto Bind(std::string&& ip, uint16_t port) {
    struct Data {
      Acceptor* acceptor;
      std::string ip;
      uint16_t port;
    };

    return Eventual<void>()
        .interruptible()
        .context(Data{this, std::move(ip), port})
        .start([](auto& data, auto& k, Interrupt::Handler& handler) {
          asio::post(
              data.acceptor->io_context(),
              [data = std::move(data),
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;
                  asio::ip::tcp::endpoint endpoint;

                  switch (data.acceptor->protocol_) {
                    case Protocol::IPV4:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v4(data.ip, ec),
                          data.port);
                      break;
                    case Protocol::IPV6:
                      endpoint = asio::ip::tcp::endpoint(
                          asio::ip::make_address_v6(data.ip, ec),
                          data.port);
                      break;
                    default:
                      k.Fail("Unknown protocol");
                      return;
                  }

                  if (ec) {
                    k.Fail(ec.message());
                    return;
                  }

                  data.acceptor->underlying_handle().bind(endpoint, ec);

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  auto Listen() {
    return Eventual<void>()
        .interruptible()
        .context(this)
        .start([](Acceptor* acceptor, auto& k, Interrupt::Handler& handler) {
          asio::post(
              acceptor->io_context(),
              [acceptor,
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;

                  acceptor->underlying_handle().listen(
                      asio::socket_base::max_listen_connections,
                      ec);

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  auto Close() {
    return Eventual<void>()
        .interruptible()
        .context(this)
        .start([](Acceptor* acceptor, auto& k, Interrupt::Handler& handler) {
          asio::post(
              acceptor->io_context(),
              [acceptor,
               &k,
               &handler]() {
                if (handler.interrupt().Triggered()) {
                  k.Stop();
                } else {
                  asio::error_code ec;

                  acceptor->underlying_handle().close(ec);

                  if (!ec) {
                    k.Start();
                  } else {
                    k.Fail(ec.message());
                  }
                }
              });
        });
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  bool IsOpen() {
    return underlying_handle().is_open();
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  uint16_t BindedPort() {
    return underlying_handle().local_endpoint().port();
  }

  // Maybe thread-unsafe.
  // If there is any other operation on acceptor
  // on a different thread, return value might be incorrect.
  std::string BindedIp() {
    return underlying_handle().local_endpoint().address().to_string();
  }

 private:
  EventLoop& loop_;
  asio::ip::tcp::acceptor acceptor_;
  Protocol protocol_ = Protocol::NOT_SET;

  asio::ip::tcp::acceptor& underlying_handle() {
    return acceptor_;
  }

  asio::io_context& io_context() {
    return loop_.io_context();
  }
};

////////////////////////////////////////////////////////////////////////

} // namespace tcp
} // namespace ip
} // namespace eventuals
