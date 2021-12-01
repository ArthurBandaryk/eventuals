#include "event-loop-test.h"
#include "eventuals/tcp.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gtest/gtest.h"

class TCPTest : public EventLoopTest {};

using eventuals::EventLoop;
using eventuals::Terminate;
using eventuals::Then;

using eventuals::ip::tcp::Protocol;

using Acceptor = eventuals::ip::tcp::Acceptor;
using Socket = eventuals::ip::tcp::Socket;
#if !defined(_WIN32)
using SSL_Socket = eventuals::ip::tcp::ssl::Socket;
#endif

// Acceptor tests.

#if defined(_WIN32)

TEST_F(TCPTest, AcceptorAcceptV4WINAPI) {
  Socket sock(EventLoop::Default());
  Acceptor acceptor(EventLoop::Default());

  std::thread winsock_thread;

  EXPECT_FALSE(acceptor.IsOpen());
  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen()
        | Then([&winsock_thread, &acceptor]() {
             winsock_thread = std::thread([&acceptor]() {
               SOCKET socket_fd = INVALID_SOCKET;

               struct sockaddr_in addr = {0};
               addr.sin_family = AF_INET;
               addr.sin_port = htons(acceptor.BindedPort());
               addr.sin_addr.s_addr = inet_addr("127.0.0.1");

               socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
               EXPECT_NE(socket_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               int error = connect(
                   socket_fd,
                   (struct sockaddr*) &addr,
                   sizeof(addr));
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | acceptor.Accept(sock)
        | Then([&sock, &acceptor]() {
             EXPECT_TRUE(sock.IsOpen());
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Close()
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(acceptor.IsOpen());
  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, AcceptorAcceptV6WINAPI) {
  Socket sock(EventLoop::Default());
  Acceptor acceptor(EventLoop::Default());

  std::thread winsock_thread;

  EXPECT_FALSE(acceptor.IsOpen());
  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    return acceptor.Open(Protocol::IPV6)
        | acceptor.Bind("::1", 0)
        | acceptor.Listen()
        | Then([&winsock_thread, &acceptor]() {
             winsock_thread = std::thread([&acceptor]() {
               SOCKET socket_fd = INVALID_SOCKET;

               struct sockaddr_in6 addr = {0};
               addr.sin6_family = AF_INET6;
               addr.sin6_port = htons(acceptor.BindedPort());
               inet_pton(AF_INET6, "::1", &addr.sin6_addr);

               socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
               EXPECT_NE(socket_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               int error = connect(
                   socket_fd,
                   (struct sockaddr*) &addr,
                   sizeof(addr));
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | acceptor.Accept(sock)
        | Then([&sock, &acceptor]() {
             EXPECT_TRUE(sock.IsOpen());
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Close()
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);


  future.get();


  EXPECT_FALSE(acceptor.IsOpen());
  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}

#endif

// Socket tests without SSL.


TEST_F(TCPTest, SocketBindIPV4) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV4)
        | sock.Bind("127.0.0.1", 0)
        | Then([&sock]() {
             EXPECT_EQ(sock.BindedIp(), "127.0.0.1");
           });
  };

  auto [future, k] = Terminate(e());
  k.Start();

  future.get();

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindAnyIPV4) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV4)
        | sock.Bind("0.0.0.0", 0)
        | Then([&sock]() {
             EXPECT_EQ(sock.BindedIp(), "0.0.0.0");
           });
  };

  auto [future, k] = Terminate(e());
  k.Start();

  future.get();

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindBadIPV4Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV4)
        | sock.Bind("0.0.0.256", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV6ToV4Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV4)
        | sock.Bind("::1", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV6) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Bind("::1", 0)
        | Then([&sock]() {
             EXPECT_EQ(sock.BindedIp(), "::1");
           });
  };

  auto [future, k] = Terminate(e());
  k.Start();

  future.get();

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindAnyIPV6) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Bind("::", 0)
        | Then([&sock]() {
             EXPECT_EQ(sock.BindedIp(), "::");
           });
  };

  auto [future, k] = Terminate(e());
  k.Start();

  future.get();

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindBadIPV6Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Bind("::H", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV4ToV6Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Bind("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketBindClosedFail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Bind("0.0.0.0", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(sock.IsOpen());
}


TEST_F(TCPTest, SocketClose) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV4)
        | Then([&sock]() {
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Bind("127.0.0.1", 0)
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  future.get();

  EXPECT_FALSE(sock.IsOpen());
}


#if defined(_WIN32)

TEST_F(TCPTest, SocketConnectV4WINAPI) {
  Socket sock(EventLoop::Default());

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&winsock_thread, &socket_fd]() {
             winsock_thread = std::thread([&socket_fd]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | Then([&sock]() {
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, SocketConnectV6WINAPI) {
  Socket sock(EventLoop::Default());

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&winsock_thread, &socket_fd]() {
             winsock_thread = std::thread([&socket_fd]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("::1", socket_port)
        | Then([&sock]() {
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, SocketReceiveV4WINAPI) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&winsock_thread, &socket_fd, &data]() {
             winsock_thread = std::thread([&socket_fd, &data]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               size_t bytes_to_write = data.size();
               size_t bytes_written = 0;
               while (bytes_written < bytes_to_write) {
                 char* buffer_ptr = data.data()
                     + bytes_written;

                 int bytes_written_this_send = send(
                     client_fd,
                     buffer_ptr,
                     bytes_to_write - bytes_written,
                     0);
                 EXPECT_NE(bytes_written_this_send, SOCKET_ERROR)
                     << "Error code: "
                     << WSAGetLastError();

                 bytes_written += bytes_written_this_send;
               }
               EXPECT_EQ(bytes_written, bytes_to_write);

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | sock.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&sock, &data, &buffer](int&& bytes_received) {
             EXPECT_EQ(data.size(), bytes_received);
             EXPECT_EQ(data, buffer);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, SocketReceiveV6WINAPI) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&winsock_thread, &socket_fd, &data]() {
             winsock_thread = std::thread([&socket_fd, &data]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               size_t bytes_to_write = data.size();
               size_t bytes_written = 0;
               while (bytes_written < bytes_to_write) {
                 char* buffer_ptr = data.data()
                     + bytes_written;

                 int bytes_written_this_send = send(
                     client_fd,
                     buffer_ptr,
                     bytes_to_write - bytes_written,
                     0);
                 EXPECT_NE(bytes_written_this_send, SOCKET_ERROR)
                     << "Error code: "
                     << WSAGetLastError();

                 bytes_written += bytes_written_this_send;
               }
               EXPECT_EQ(bytes_written, bytes_to_write);

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("::1", socket_port)
        | sock.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&sock, &data, &buffer](int&& bytes_received) {
             EXPECT_EQ(data.size(), bytes_received);
             EXPECT_EQ(data, buffer);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, SocketSendV4WINAPI) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&winsock_thread, &socket_fd, &data, &buffer]() {
             winsock_thread = std::thread([&socket_fd, &data, &buffer]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               size_t bytes_to_read = data.size();
               size_t bytes_read = 0;
               while (bytes_read < bytes_to_read) {
                 char* buffer_ptr = const_cast<char*>(buffer.data())
                     + bytes_read;

                 int bytes_read_this_recv = recv(
                     client_fd,
                     buffer_ptr,
                     bytes_to_read - bytes_read,
                     0);
                 EXPECT_NE(bytes_read_this_recv, SOCKET_ERROR)
                     << "Error code: "
                     << WSAGetLastError();

                 bytes_read += bytes_read_this_recv;
               }
               EXPECT_EQ(bytes_read, bytes_to_read);
               EXPECT_EQ(data, buffer);

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | sock.Send(
            data.data(),
            data.size())
        | Then([&sock, &data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}


TEST_F(TCPTest, SocketSendV6WINAPI) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread winsock_thread;
  SOCKET socket_fd = INVALID_SOCKET;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, INVALID_SOCKET)
        << "Error code: "
        << WSAGetLastError();

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    error = listen(socket_fd, 1);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    ZeroMemory(&addr, sizeof(addr));
    int addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_NE(error, SOCKET_ERROR)
        << "Error code: "
        << WSAGetLastError();

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&winsock_thread, &socket_fd, &data, &buffer]() {
             winsock_thread = std::thread([&socket_fd, &data, &buffer]() {
               SOCKET client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, INVALID_SOCKET)
                   << "Error code: "
                   << WSAGetLastError();

               size_t bytes_to_read = data.size();
               size_t bytes_read = 0;
               while (bytes_read < bytes_to_read) {
                 char* buffer_ptr = const_cast<char*>(buffer.data())
                     + bytes_read;

                 int bytes_read_this_recv = recv(
                     client_fd,
                     buffer_ptr,
                     bytes_to_read - bytes_read,
                     0);
                 EXPECT_NE(bytes_read_this_recv, SOCKET_ERROR)
                     << "Error code: "
                     << WSAGetLastError();

                 bytes_read += bytes_read_this_recv;
               }
               EXPECT_EQ(bytes_read, bytes_to_read);
               EXPECT_EQ(data, buffer);

               int error = closesocket(client_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();

               error = closesocket(socket_fd);
               EXPECT_NE(error, SOCKET_ERROR)
                   << "Error code: "
                   << WSAGetLastError();
             });
           })
        | sock.Connect("::1", socket_port)
        | sock.Send(
            data.data(),
            data.size())
        | Then([&sock, &data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  winsock_thread.join();
}

#else

TEST_F(TCPTest, SocketConnectV4POSIX) {
  Socket sock(EventLoop::Default());

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&posix_thread, &socket_fd]() {
             posix_thread = std::thread([&socket_fd]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | Then([&sock]() {
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}


TEST_F(TCPTest, SocketConnectV6POSIX) {
  Socket sock(EventLoop::Default());

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&posix_thread, &socket_fd]() {
             posix_thread = std::thread([&socket_fd]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("::1", socket_port)
        | Then([&sock]() {
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}


TEST_F(TCPTest, SocketReceiveV4POSIX) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&posix_thread, &socket_fd, &data]() {
             posix_thread = std::thread([&socket_fd, &data]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               size_t bytes_to_write = data.size();
               size_t bytes_written = 0;
               while (bytes_written < bytes_to_write) {
                 char* buffer_ptr = data.data()
                     + bytes_written;

                 int bytes_written_this_send = send(
                     client_fd,
                     buffer_ptr,
                     bytes_to_write - bytes_written,
                     0);
                 EXPECT_NE(bytes_written_this_send, -1) << strerror(errno);

                 bytes_written += bytes_written_this_send;
               }
               EXPECT_EQ(bytes_written, bytes_to_write);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | sock.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&sock, &data, &buffer](int&& bytes_received) {
             EXPECT_EQ(data.size(), bytes_received);
             EXPECT_EQ(data, buffer);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}


TEST_F(TCPTest, SocketReceiveV6POSIX) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&posix_thread, &socket_fd, &data]() {
             posix_thread = std::thread([&socket_fd, &data]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               size_t bytes_to_write = data.size();
               size_t bytes_written = 0;
               while (bytes_written < bytes_to_write) {
                 char* buffer_ptr = data.data()
                     + bytes_written;

                 int bytes_written_this_send = send(
                     client_fd,
                     buffer_ptr,
                     bytes_to_write - bytes_written,
                     0);
                 EXPECT_NE(bytes_written_this_send, -1) << strerror(errno);

                 bytes_written += bytes_written_this_send;
               }
               EXPECT_EQ(bytes_written, bytes_to_write);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("::1", socket_port)
        | sock.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&sock, &data, &buffer](int&& bytes_received) {
             EXPECT_EQ(data.size(), bytes_received);
             EXPECT_EQ(data, buffer);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}


TEST_F(TCPTest, SocketSendV4POSIX) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin_port);

    return sock.Open(Protocol::IPV4)
        | Then([&posix_thread, &socket_fd, &data, &buffer]() {
             posix_thread = std::thread([&socket_fd, &data, &buffer]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               size_t bytes_to_read = data.size();
               size_t bytes_read = 0;
               while (bytes_read < bytes_to_read) {
                 char* buffer_ptr = const_cast<char*>(buffer.data())
                     + bytes_read;

                 int bytes_read_this_recv = recv(
                     client_fd,
                     buffer_ptr,
                     bytes_to_read - bytes_read,
                     0);
                 EXPECT_NE(bytes_read_this_recv, -1) << strerror(errno);

                 bytes_read += bytes_read_this_recv;
               }
               EXPECT_EQ(bytes_read, bytes_to_read);
               EXPECT_EQ(data, buffer);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("127.0.0.1", socket_port)
        | sock.Send(
            data.data(),
            data.size())
        | Then([&sock, &data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}


TEST_F(TCPTest, SocketSendV6POSIX) {
  Socket sock(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  std::thread posix_thread;
  int socket_fd = -1;
  uint16_t socket_port = 0;

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);

    socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(socket_fd, -1) << strerror(errno);

    int error = bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    EXPECT_EQ(error, 0) << strerror(errno);

    error = listen(socket_fd, 1);
    EXPECT_EQ(error, 0) << strerror(errno);

    memset(&addr, 0, sizeof(addr));
    socklen_t addr_size = sizeof(addr);
    error = getsockname(
        socket_fd,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_size);
    EXPECT_EQ(error, 0) << strerror(errno);

    socket_port = ntohs(addr.sin6_port);

    return sock.Open(Protocol::IPV6)
        | Then([&posix_thread, &socket_fd, &data, &buffer]() {
             posix_thread = std::thread([&socket_fd, &data, &buffer]() {
               int client_fd = accept(socket_fd, nullptr, nullptr);
               EXPECT_NE(client_fd, -1) << strerror(errno);

               size_t bytes_to_read = data.size();
               size_t bytes_read = 0;
               while (bytes_read < bytes_to_read) {
                 char* buffer_ptr = const_cast<char*>(buffer.data())
                     + bytes_read;

                 int bytes_read_this_recv = recv(
                     client_fd,
                     buffer_ptr,
                     bytes_to_read - bytes_read,
                     0);
                 EXPECT_NE(bytes_read_this_recv, -1) << strerror(errno);

                 bytes_read += bytes_read_this_recv;
               }
               EXPECT_EQ(bytes_read, bytes_to_read);
               EXPECT_EQ(data, buffer);

               int error = close(client_fd);
               EXPECT_EQ(error, 0) << strerror(errno);

               error = close(socket_fd);
               EXPECT_EQ(error, 0) << strerror(errno);
             });
           })
        | sock.Connect("::1", socket_port)
        | sock.Send(
            data.data(),
            data.size())
        | Then([&sock, &data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
             EXPECT_TRUE(sock.IsOpen());
           })
        | sock.Close();
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  future.get();

  EXPECT_FALSE(sock.IsOpen());

  posix_thread.join();
}

#endif


TEST_F(TCPTest, SocketConnectBadIPV4Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&]() {
    return sock.Open(Protocol::IPV4)
        | sock.Connect("127.0.0.256", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketConnectBadIPV6Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Connect("::H", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketConnectIPV4ToV6Fail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Open(Protocol::IPV6)
        | sock.Connect("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(sock.IsOpen());
}


TEST_F(TCPTest, SocketConnectClosedFail) {
  Socket sock(EventLoop::Default());

  EXPECT_FALSE(sock.IsOpen());

  auto e = [&sock]() {
    return sock.Connect("0.0.0.0", 0);
  };

  auto [future, k] = Terminate(e());
  k.Start();

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(sock.IsOpen());
}


// Socket tests with SSL.

// General tests.

TEST_F(TCPTest, ClientAndServerV4) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV4)
        | client.Connect("127.0.0.1", acceptor.BindedPort())
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}


TEST_F(TCPTest, ClientAndServerV6) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV6)
        | acceptor.Bind("::1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV6)
        | client.Connect("::1", acceptor.BindedPort())
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}


TEST_F(TCPTest, ClientAndServerV4Transfer) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV4)
        | client.Connect("127.0.0.1", acceptor.BindedPort())
        | client.Send(
            data.data(),
            data.size())
        | Then([&data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
           })
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | accepted.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&data, &buffer](int&& bytes_read) {
             EXPECT_EQ(data.size(), bytes_read);
             EXPECT_EQ(data, buffer);
           })
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}


TEST_F(TCPTest, ClientAndServerV6Transfer) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV6)
        | acceptor.Bind("::1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV6)
        | client.Connect("::1", acceptor.BindedPort())
        | client.Send(
            data.data(),
            data.size())
        | Then([&data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
           })
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | accepted.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&data, &buffer](int&& bytes_read) {
             EXPECT_EQ(data.size(), bytes_read);
             EXPECT_EQ(data, buffer);
           })
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}


TEST_F(TCPTest, ClientAndServerV4Echo) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');
  std::string buffer_echo(data.size(), '0');

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV4)
        | client.Connect("127.0.0.1", acceptor.BindedPort())
        | client.Send(
            data.data(),
            data.size())
        | Then([&data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
           })
        | client.Receive(
            const_cast<char*>(buffer_echo.data()),
            buffer_echo.size(),
            buffer.size())
        | Then([&buffer, &buffer_echo](int&& bytes_read) {
             EXPECT_EQ(buffer.size(), bytes_read);
             EXPECT_EQ(buffer, buffer_echo);
           })
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | accepted.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&data, &buffer](int&& bytes_read) {
             EXPECT_EQ(data.size(), bytes_read);
             EXPECT_EQ(data, buffer);
           })
        | accepted.Send(
            buffer.data(),
            buffer.size())
        | Then([&buffer](int&& bytes_sent) {
             EXPECT_EQ(buffer.size(), bytes_sent);
           })
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}


TEST_F(TCPTest, ClientAndServerV6Echo) {
  Acceptor acceptor(EventLoop::Default());
  Socket client(EventLoop::Default());
  Socket accepted(EventLoop::Default());

  std::string data = "Hello World";
  std::string buffer(data.size(), '0');
  std::string buffer_echo(data.size(), '0');

  auto server_start_listening_e = [&]() {
    return acceptor.Open(Protocol::IPV6)
        | acceptor.Bind("::1", 0)
        | acceptor.Listen();
  };

  auto [server_listen_future, server_listen_k] = Terminate(
      server_start_listening_e());
  server_listen_k.Start();
  server_listen_future.get();

  auto client_e = [&]() {
    return client.Open(Protocol::IPV6)
        | client.Connect("::1", acceptor.BindedPort())
        | client.Send(
            data.data(),
            data.size())
        | Then([&data](int&& bytes_sent) {
             EXPECT_EQ(data.size(), bytes_sent);
           })
        | client.Receive(
            const_cast<char*>(buffer_echo.data()),
            buffer_echo.size(),
            buffer.size())
        | Then([&buffer, &buffer_echo](int&& bytes_read) {
             EXPECT_EQ(buffer.size(), bytes_read);
             EXPECT_EQ(buffer, buffer_echo);
           })
        | client.Close();
  };

  auto server_e = [&]() {
    return acceptor.Accept(accepted)
        | accepted.Receive(
            const_cast<char*>(buffer.data()),
            buffer.size(),
            data.size())
        | Then([&data, &buffer](int&& bytes_read) {
             EXPECT_EQ(data.size(), bytes_read);
             EXPECT_EQ(data, buffer);
           })
        | accepted.Send(
            buffer.data(),
            buffer.size())
        | Then([&buffer](int&& bytes_sent) {
             EXPECT_EQ(buffer.size(), bytes_sent);
           })
        | acceptor.Close()
        | accepted.Close();
  };

  auto [client_future, client_k] = Terminate(client_e());
  auto [server_future, server_k] = Terminate(server_e());

  client_k.Start();
  server_k.Start();

  EventLoop::Default().RunUntil(server_future);
  EventLoop::Default().RunUntil(client_future);

  server_future.get();
  client_future.get();
}
