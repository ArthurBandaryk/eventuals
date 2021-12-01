#include "eventuals/tcp.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tcp.h"

using eventuals::EventLoop;
using eventuals::Terminate;
using eventuals::Then;

using eventuals::ip::tcp::Acceptor;
using eventuals::ip::tcp::Protocol;
using eventuals::ip::tcp::Socket;

using testing::MockFunction;


TEST_F(TCPTest, SocketConnectIPV4Success) {
}


TEST_F(TCPTest, SocketConnectIPV6Success) {
}


TEST_F(TCPTest, SocketConnectIPV4Fail) {
}


TEST_F(TCPTest, SocketConnectIPV6Fail) {
}


TEST_F(TCPTest, SocketConnectIPV4Interrupt) {
  Acceptor acceptor;
  Socket socket;

  EXPECT_FALSE(acceptor.IsOpen());
  EXPECT_FALSE(socket.IsOpen());

  eventuals::Interrupt interrupt;

  MockFunction<void()> unreachable_func;

  EXPECT_CALL(unreachable_func, Call()).Times(0);

  auto e_setup = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen()
        | socket.Open(Protocol::IPV4)
        | Then([&]() {
             EXPECT_TRUE(acceptor.IsOpen());
             EXPECT_TRUE(socket.IsOpen());
           });
  };

  auto [future_setup, k_setup] = Terminate(e_setup());

  k_setup.Start();

  EventLoop::Default().RunUntil(future_setup);

  EXPECT_NO_THROW(future_setup.get());

  auto e = [&]() {
    return socket.Connect("127.0.0.1", acceptor.BindedPort())
        | Then([&]() {
             unreachable_func.Call();
           });
  };

  auto [future, k] = Terminate(e());

  k.Start();

  interrupt.Trigger();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_TRUE(acceptor.IsOpen());
  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketConnectIPV6Interrupt) {
}
