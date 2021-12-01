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


TEST_F(TCPTest, SocketBindIPV4Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("127.0.0.1", 0)
        | Then([&socket]() {
             EXPECT_EQ(socket.BindedIp(), "127.0.0.1");
             EXPECT_GT(socket.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV6Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV6)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("::1", 0)
        | Then([&socket]() {
             EXPECT_EQ(socket.BindedIp(), "::1");
             EXPECT_GT(socket.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindAnyIPV4Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("0.0.0.0", 0)
        | Then([&socket]() {
             EXPECT_EQ(socket.BindedIp(), "0.0.0.0");
             EXPECT_GT(socket.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindAnyIPV6Success) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV6)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("::", 0)
        | Then([&socket]() {
             EXPECT_EQ(socket.BindedIp(), "::");
             EXPECT_GT(socket.BindedPort(), 0);
           });
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_NO_THROW(future.get());

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindBadIPV4Fail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("0.0.0.256", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV6ToV4Fail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV4)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("::1", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindBadIPV6Fail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV6)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("::H", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindIPV4ToV6Fail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Open(Protocol::IPV6)
        | Then([&socket]() {
             EXPECT_TRUE(socket.IsOpen());
           })
        | socket.Bind("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), std::string);

  EXPECT_TRUE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindClosedFail) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  auto e = [&socket]() {
    return socket.Bind("0.0.0.0", 0);
  };

  auto [future, k] = Terminate(e());

  eventuals::Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), const char*);

  EXPECT_FALSE(socket.IsOpen());
}


TEST_F(TCPTest, SocketBindInterrupt) {
  Socket socket;

  EXPECT_FALSE(socket.IsOpen());

  eventuals::Interrupt interrupt;

  auto e = [&]() {
    return socket.Open(Protocol::IPV4)
        | Then([&]() {
             EXPECT_TRUE(socket.IsOpen());
             interrupt.Trigger();
           })
        | socket.Bind("127.0.0.1", 0);
  };

  auto [future, k] = Terminate(e());

  k.Register(interrupt);

  k.Start();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);

  EXPECT_TRUE(socket.IsOpen());
  EXPECT_NE(socket.BindedIp(), "127.0.0.1");
  EXPECT_EQ(socket.BindedPort(), 0);
}
