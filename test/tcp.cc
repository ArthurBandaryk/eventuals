#include "eventuals/tcp.h"

#include "eventuals/just.h"
#include "eventuals/let.h"
#include "eventuals/terminal.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/event-loop-test.h"

using eventuals::EventLoop;
using eventuals::Interrupt;
using eventuals::Just;
using eventuals::Let;
using eventuals::Terminate;
using eventuals::Then;

using eventuals::ip::tcp::AcceptOnce;

class TCPTest : public EventLoopTest {};

// NOTE: Windows has different socket API, so we have 2 sets of tests.
#if !defined(_WIN32)

TEST_F(TCPTest, AcceptOnceSucceed) {
  auto e = AcceptOnce("0.0.0.0", 50000)
      | Then(Let([](auto& client_socket) {
             return Then([&]() {
               return client_socket.Close();
             });
           }));

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EXPECT_EQ(
      std::future_status::timeout,
      future.wait_for(std::chrono::seconds(0)));

  EventLoop::Waiter waiter(&EventLoop::Default(), "connect");

  EventLoop::Default().Submit(
      []() {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(50000);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(socket_fd, -1) << strerror(errno);
        int status = connect(socket_fd, (struct sockaddr*) &addr, sizeof(addr));
        EXPECT_EQ(status, 0) << strerror(errno);
        status = close(socket_fd);
        EXPECT_EQ(status, 0) << strerror(errno);
      },
      &waiter);

  EventLoop::Default().Run();

  future.get();
}

TEST_F(TCPTest, AcceptOnceInvalidPortFail) {
  auto e = AcceptOnce("0.0.0.0", 80000);

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), const char*);
}

TEST_F(TCPTest, AcceptOnceInvalidAddressFail) {
  auto e = AcceptOnce("0.0.0.256", 80000);

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), const char*);
}

TEST_F(TCPTest, AcceptOnceInterrupt) {
  auto e = AcceptOnce("0.0.0.0", 50000);

  auto [future, k] = Terminate(std::move(e));

  Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  interrupt.Trigger();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}

TEST_F(TCPTest, WriteSucceed) {
  std::string some_data = "Hello World!";
  int socket_fd = -1;

  auto e = AcceptOnce("0.0.0.0", 50000)
      | Then(Let([&](auto& client_socket) {
             return Then([&]() {
               return client_socket.Write(some_data)
                   | Then([&]() {
                        std::string data;
                        data.resize(some_data.size());
                        int bytes_read = read(socket_fd, const_cast<char*>(data.data()), some_data.size());
                        EXPECT_NE(bytes_read, -1) << strerror(errno);
                        EXPECT_EQ(data, some_data);
                        int status = close(socket_fd);
                        EXPECT_EQ(status, 0) << strerror(errno);
                      })
                   | client_socket.Close();
             });
           }));

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EXPECT_EQ(
      std::future_status::timeout,
      future.wait_for(std::chrono::seconds(0)));

  EventLoop::Waiter waiter(&EventLoop::Default(), "connect");

  EventLoop::Default().Submit(
      [&socket_fd]() {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(50000);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(socket_fd, -1) << strerror(errno);
        int status = connect(socket_fd, (struct sockaddr*) &addr, sizeof(addr));
        EXPECT_EQ(status, 0) << strerror(errno);
      },
      &waiter);

  EventLoop::Default().Run();

  future.get();
}

TEST_F(TCPTest, ReadSucceed) {
  std::string some_data = "Hello World!";
  int socket_fd = -1;

  auto e = AcceptOnce("0.0.0.0", 50000)
      | Then(Let([&](auto& client_socket) {
             return Then([&]() {
               return Then([&]() {
                        int status = write(socket_fd, some_data.data(), some_data.size());
                        EXPECT_NE(status, -1) << strerror(errno);
                      })
                   | client_socket.Read(some_data.size())
                   | Then([&](auto&& data) {
                        EXPECT_EQ(data, some_data);
                        int status = close(socket_fd);
                        EXPECT_EQ(status, 0) << strerror(errno);
                      })
                   | client_socket.Close();
             });
           }));

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EXPECT_EQ(
      std::future_status::timeout,
      future.wait_for(std::chrono::seconds(0)));

  EventLoop::Waiter waiter(&EventLoop::Default(), "connect");

  EventLoop::Default().Submit(
      [&socket_fd]() {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(50000);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(socket_fd, -1) << strerror(errno);
        int status = connect(socket_fd, (struct sockaddr*) &addr, sizeof(addr));
        EXPECT_EQ(status, 0) << strerror(errno);
      },
      &waiter);

  EventLoop::Default().Run();

  future.get();
}

#else

TEST_F(TCPTest, AcceptSucceed) {
  auto e = AcceptOnce("0.0.0.0", 50000)
      | Then(Let([](auto& client_socket) {
             return Then([&]() {
               return client_socket.Close();
             });
           }));

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EXPECT_EQ(
      std::future_status::timeout,
      future.wait_for(std::chrono::seconds(0)));

  EventLoop::Waiter waiter(&EventLoop::Default(), "connect");

  EventLoop::Default().Submit(
      []() {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(50000);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        SOCKET socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(socket_fd, INVALID_SOCKET) << "Error code: " << WSAGetLastError();
        int status = connect(socket_fd, (struct sockaddr*) &addr, sizeof(addr));
        EXPECT_NE(status, SOCKET_ERROR) << "Error code: " << WSAGetLastError();
        status = closesocket(socket_fd);
        EXPECT_NE(status, SOCKET_ERROR) << "Error code: " << WSAGetLastError();
      },
      &waiter);

  EventLoop::Default().Run();

  future.get();
}

TEST_F(TCPTest, AcceptOnceInvalidPortFail) {
  auto e = AcceptOnce("0.0.0.0", 80000);

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), const char*);
}

TEST_F(TCPTest, AcceptOnceInvalidAddressFail) {
  auto e = AcceptOnce("0.0.0.256", 80000);

  auto [future, k] = Terminate(std::move(e));
  k.Start();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), const char*);
}

TEST_F(TCPTest, AcceptInterrupt) {
  // NOTE: Current std::future implementation in MSVC STL requires assignment
  // operators to be not deleted for its underlying type.
  // Using Just will let us bypass this restriction by changing the underlying
  // type of std::future to void.
  auto e = AcceptOnce("0.0.0.0", 50000)
      | Just();

  auto [future, k] = Terminate(std::move(e));

  Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  interrupt.Trigger();

  EventLoop::Default().Run();

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}

#endif