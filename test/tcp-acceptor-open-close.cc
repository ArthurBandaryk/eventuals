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


TEST_F(TCPTest, AcceptorOpenCloseV4Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Close()
        | Then([&acceptor]() {
             EXPECT_FALSE(acceptor.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorOpenCloseV6Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV6)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Close()
        | Then([&acceptor]() {
             EXPECT_FALSE(acceptor.IsOpen());
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorOpenBadProtocolFail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::NOT_SET);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(acceptor.IsOpen());
}


// TODO(folming): Implement AcceptorCloseFail test.
// Closing an already closed one will not call .Fail();


TEST_F(TCPTest, AcceptorOpenInterrupt) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  interrupt.Trigger();

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_FALSE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorCloseInterrupt) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  eventuals::Interrupt interrupt;

  auto e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&]() {
             EXPECT_TRUE(acceptor.IsOpen());
             interrupt.Trigger();
           })
        | acceptor.Close();
  };

  auto [future, k] = Terminate(e());

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_TRUE(acceptor.IsOpen());
}
