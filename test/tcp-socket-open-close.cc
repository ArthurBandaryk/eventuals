#include "eventuals/tcp.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gtest/gtest.h"
#include "tcp.h"

using eventuals::EventLoop;
using eventuals::Terminate;
using eventuals::Then;

using eventuals::ip::tcp::Protocol;
using eventuals::ip::tcp::Socket;


TEST_F(TCPTest, SocketOpenCloseV4Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Close()
        | Then([&socket]() {
             EXPECT_FALSE(socket.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, SocketOpenCloseV6Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV6)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Close()
        | Then([&socket]() {
             EXPECT_FALSE(socket.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, SocketOpenBadProtocolFail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::NOT_SET);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(socket.IsOpen());
}


TEST_F(TCPTest, SocketOpenInterrupt) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  interrupt.Trigger();

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_FALSE(socket.IsOpen());
}


TEST_F(TCPTest, SocketCloseInterrupt) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  eventuals::Interrupt interrupt;

  auto e = [&]() {
    return socket.Open(Protocol::IPV4)
        | Then([&]() {
             EXPECT_TRUE(socket.IsOpen());
             interrupt.Trigger();
           })
        | socket.Close();
  };

  auto [future, k] = Terminate(e());

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}
