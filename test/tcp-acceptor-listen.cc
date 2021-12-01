#include "eventuals/tcp.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gtest/gtest.h"
#include "tcp.h"

using eventuals::EventLoop;
using eventuals::Terminate;
using eventuals::Then;

using eventuals::ip::tcp::Acceptor;
using eventuals::ip::tcp::Protocol;


TEST_F(TCPTest, AcceptorListenSuccess) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("127.0.0.1", 0)
        | acceptor.Listen()
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorListenFail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Listen()
        | Then([&acceptor]() {
             EXPECT_FALSE(acceptor.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);
}


TEST_F(TCPTest, AcceptorListenInterrupt) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  eventuals::Interrupt interrupt;

  auto e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("127.0.0.1", 0)
        | Then([&interrupt]() {
             interrupt.Trigger();
           })
        | acceptor.Listen();
  };

  auto [future, k] = Terminate(e());

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_TRUE(acceptor.IsOpen());
}
