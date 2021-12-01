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


TEST_F(TCPTest, AcceptorBindIPV4Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("127.0.0.1", 0)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
             EXPECT_EQ(acceptor.BindedIp(), "127.0.0.1");
             EXPECT_GT(acceptor.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorBindIPV6Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV6)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("::1", 0)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
             EXPECT_EQ(acceptor.BindedIp(), "::1");
             EXPECT_GT(acceptor.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorBindAnyIPV4Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("0.0.0.0", 0)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
             EXPECT_EQ(acceptor.BindedIp(), "0.0.0.0");
             EXPECT_GT(acceptor.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorBindAnyIPV6Success) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV6)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("::", 0)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
             EXPECT_EQ(acceptor.BindedIp(), "::");
             EXPECT_GT(acceptor.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());
}


TEST_F(TCPTest, AcceptorBindBadIPV4Fail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("0.0.0.256", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorBindIPV6ToV4Fail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("::1", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorBindBadIPV6Fail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV6)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("::H", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorBindIPV4ToV6Fail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Open(Protocol::IPV6)
        | Then([&acceptor]() {
             EXPECT_TRUE(acceptor.IsOpen());
           })
        | acceptor.Bind("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorBindClosedFail) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  auto e = [&acceptor]() {
    return acceptor.Bind("0.0.0.0", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(acceptor.IsOpen());
}


TEST_F(TCPTest, AcceptorBindInterrupt) {
  Acceptor acceptor;

  EXPECT_FALSE(acceptor.IsOpen());

  eventuals::Interrupt interrupt;

  auto e = [&]() {
    return acceptor.Open(Protocol::IPV4)
        | Then([&]() {
             EXPECT_TRUE(acceptor.IsOpen());
             interrupt.Trigger();
           })
        | acceptor.Bind("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_TRUE(acceptor.IsOpen());
  EXPECT_NE(acceptor.BindedIp(), "127.0.0.1");
  EXPECT_EQ(acceptor.BindedPort(), 0);
}
