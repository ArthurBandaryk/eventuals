#include "eventuals/stream.h"

#include <thread>

#include "eventuals/head.h"
#include "eventuals/lazy.h"
#include "eventuals/loop.h"
#include "eventuals/map.h"
#include "eventuals/raise.h"
#include "eventuals/reduce.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/expect-throw-what.h"

using eventuals::Eventual;
using eventuals::Head;
using eventuals::Interrupt;
using eventuals::Lazy;
using eventuals::Loop;
using eventuals::Map;
using eventuals::Raise;
using eventuals::Reduce;
using eventuals::Stream;
using eventuals::Terminate;
using eventuals::Then;

using testing::MockFunction;

TEST(StreamTest, Succeed) {
  // Using mocks to ensure fail and stop callbacks don't get invoked.
  MockFunction<void()> fail, stop, done;

  EXPECT_CALL(fail, Call())
      .Times(0);

  EXPECT_CALL(stop, Call())
      .Times(0);

  EXPECT_CALL(done, Call())
      .Times(0);

  auto s = [&]() {
    return Stream<int>()
               .context(5)
               .next([](auto& count, auto& k) {
                 if (count > 0) {
                   k.Emit(count--);
                 } else {
                   k.Ended();
                 }
               })
               .done([&](auto&, auto&) {
                 done.Call();
               })
        | Loop<int>()
              .context(0)
              .body([](auto& sum, auto& stream, auto&& value) {
                sum += value;
                stream.Next();
              })
              .ended([](auto& sum, auto& k) {
                k.Start(sum);
              })
              .fail([&](auto&, auto&, auto&&) {
                fail.Call();
              })
              .stop([&](auto&, auto&) {
                stop.Call();
              });
  };

  EXPECT_EQ(15, *s());
}


TEST(StreamTest, Done) {
  // Using mocks to ensure fail and stop callbacks don't get invoked.
  MockFunction<void()> fail, stop;

  EXPECT_CALL(fail, Call())
      .Times(0);

  EXPECT_CALL(stop, Call())
      .Times(0);

  auto s = [&]() {
    return Stream<int>()
               .context(0)
               .next([](auto& value, auto& k) {
                 k.Emit(value);
               })
               .done([](auto&, auto& k) {
                 k.Ended();
               })
        | Loop<int>()
              .context(0)
              .body([](auto& count, auto& stream, auto&&) {
                if (++count == 2) {
                  stream.Done();
                } else {
                  stream.Next();
                }
              })
              .ended([](auto& count, auto& k) {
                k.Start(count);
              })
              .fail([&](auto&, auto&, auto&&) {
                fail.Call();
              })
              .stop([&](auto&, auto&) {
                stop.Call();
              });
  };

  EXPECT_EQ(2, *s());
}


TEST(StreamTest, Fail) {
  // Using mocks to ensure fail and stop callbacks don't get invoked.
  MockFunction<void()> stop, done, fail, ended;

  EXPECT_CALL(stop, Call())
      .Times(0);

  EXPECT_CALL(done, Call())
      .Times(0);

  EXPECT_CALL(fail, Call())
      .Times(0);

  EXPECT_CALL(ended, Call())
      .Times(0);

  auto s = [&]() {
    return Stream<int>()
               .context("error")
               .raises<std::runtime_error>()
               .next([](auto& error, auto& k) {
                 k.Fail(std::runtime_error(error));
               })
               .done([&](auto&, auto&) {
                 done.Call();
               })
        | Loop<int>()
              .context(0)
              .raises<std::runtime_error>()
              .body([](auto&, auto& stream, auto&&) {
                stream.Next();
              })
              .ended([&](auto&, auto&) {
                ended.Call();
              })
              .fail([&](auto&, auto& k, auto&& error) {
                k.Fail(std::forward<decltype(error)>(error));
              })
              .stop([&](auto&, auto&) {
                stop.Call();
              });
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          decltype(s())::ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  EXPECT_THROW_WHAT(*s(), "error");
}


TEST(StreamTest, InterruptStream) {
  // Using mocks to ensure fail and stop callbacks don't get invoked.
  MockFunction<void()> done, fail, ended;

  EXPECT_CALL(done, Call())
      .Times(0);

  EXPECT_CALL(fail, Call())
      .Times(0);

  EXPECT_CALL(ended, Call())
      .Times(0);

  std::atomic<bool> triggered = false;

  auto s = [&]() {
    return Stream<int>()
               .context(Lazy<std::atomic<bool>>(false))
               .interruptible()
               .begin([](auto& interrupted,
                         auto& k,
                         Interrupt::Handler& handler) {
                 handler.Install([&interrupted]() {
                   interrupted->store(true);
                 });
                 k.Begin();
               })
               .next([](auto& interrupted, auto& k) {
                 if (!interrupted->load()) {
                   k.Emit(0);
                 } else {
                   k.Stop();
                 }
               })
               .done([&](auto&, auto&) {
                 done.Call();
               })
        | Loop<int>()
              .body([&](auto& k, auto&&) {
                auto thread = std::thread(
                    [&]() mutable {
                      while (!triggered.load()) {
                        std::this_thread::yield();
                      }
                      k.Next();
                    });
                thread.detach();
              })
              .ended([&](auto&) {
                ended.Call();
              })
              .fail([&](auto&, auto&&) {
                fail.Call();
              })
              .stop([](auto& k) {
                k.Stop();
              });
  };

  auto [future, k] = Terminate(s());

  Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  interrupt.Trigger();

  triggered.store(true);

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}


TEST(StreamTest, InterruptLoop) {
  // Using mocks to ensure fail and stop callbacks don't get invoked.
  MockFunction<void()> stop, fail, body;

  EXPECT_CALL(stop, Call())
      .Times(0);

  EXPECT_CALL(fail, Call())
      .Times(0);

  std::atomic<bool> triggered = false;

  auto s = [&]() {
    return Stream<int>()
               .next([](auto& k) {
                 k.Emit(0);
               })
               .done([](auto& k) {
                 k.Ended();
               })
        | Loop<int>()
              .context(Lazy<std::atomic<bool>>(false))
              .interruptible()
              .raises<std::runtime_error>()
              .begin([](auto& interrupted,
                        auto& k,
                        Interrupt::Handler& handler) {
                handler.Install([&interrupted]() {
                  interrupted->store(true);
                });
                k.Next();
              })
              .body([&](auto&, auto& k, auto&&) {
                auto thread = std::thread(
                    [&]() mutable {
                      while (!triggered.load()) {
                        std::this_thread::yield();
                      }
                      k.Done();
                    });
                thread.detach();
              })
              .ended([](auto& interrupted, auto& k) {
                if (interrupted->load()) {
                  k.Stop();
                } else {
                  k.Fail(std::runtime_error("error"));
                }
              })
              .fail([&](auto&, auto&&) {
                fail.Call();
              })
              .stop([&](auto&) {
                stop.Call();
              });
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          decltype(s())::ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  auto [future, k] = Terminate(s());

  Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  interrupt.Trigger();

  triggered.store(true);

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}


TEST(StreamTest, InfiniteLoop) {
  auto s = []() {
    return Stream<int>()
               .context(5)
               .next([](auto& count, auto& k) {
                 if (count > 0) {
                   k.Emit(count--);
                 } else {
                   k.Ended();
                 }
               })
        | Map([](int i) { return i + 1; })
        | Loop();
  };

  *s();
}


TEST(StreamTest, MapThenLoop) {
  auto s = []() {
    return Stream<int>()
               .context(5)
               .next([](auto& count, auto& k) {
                 if (count > 0) {
                   k.Emit(count--);
                 } else {
                   k.Ended();
                 }
               })
        | Map([](int i) { return i + 1; })
        | Loop<int>()
              .context(0)
              .body([](auto& sum, auto& stream, auto&& value) {
                sum += value;
                stream.Next();
              })
              .ended([](auto& sum, auto& k) {
                k.Start(sum);
              });
  };

  EXPECT_EQ(20, *s());
}


TEST(StreamTest, MapThenReduce) {
  auto s = []() {
    return Stream<int>()
               .context(5)
               .next([](auto& count, auto& k) {
                 if (count > 0) {
                   k.Emit(count--);
                 } else {
                   k.Ended();
                 }
               })
               .done([](auto&, auto& k) {
                 k.Ended();
               })
        | Map([](int i) {
             return i + 1;
           })
        | Reduce(
               /* sum = */ 0,
               [](auto& sum) {
                 return Then([&](auto&& value) {
                   sum += value;
                   return true;
                 });
               });
  };

  EXPECT_EQ(20, *s());
}


TEST(StreamTest, Head) {
  auto s1 = []() {
    return Stream<int>()
               .next([](auto& k) {
                 k.Emit(42);
               })
        | Head();
  };

  EXPECT_EQ(42, *s1());

  auto s2 = []() {
    return Stream<int>()
               .next([](auto& k) {
                 k.Ended();
               })
        | Head();
  };

  EXPECT_THROW_WHAT(*s2(), "empty stream");
}

TEST(StreamTest, PropagateError) {
  auto e = []() {
    return Raise(std::runtime_error("error"))
        | Stream<int>()
              .next([](auto& k) {
                k.Ended();
              })
        | Head();
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          decltype(e())::ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  EXPECT_THROW_WHAT(*e(), "error");
}

TEST(StreamTest, ThrowSpecificError) {
  auto e = []() {
    return Raise(std::bad_alloc())
        | Stream<int>()
              .raises<std::runtime_error>()
              .fail([](auto& k, auto&& error) {
                static_assert(
                    std::is_same_v<
                        std::decay_t<decltype(error)>,
                        std::bad_alloc>);

                k.Fail(std::runtime_error("error"));
              })
              .next([](auto& k) {
                k.Ended();
              })
        | Head();
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          decltype(e())::ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error, std::bad_alloc>>);

  EXPECT_THROW_WHAT(*e(), "error");
}

TEST(StreamTest, ThrowGeneralError) {
  auto e = []() {
    return Raise(std::bad_alloc())
        | Stream<int>()
              .raises() // Same as 'raises<std::exception>'.
              .fail([](auto& k, auto&& error) {
                static_assert(
                    std::is_same_v<
                        std::decay_t<decltype(error)>,
                        std::bad_alloc>);

                k.Fail(std::runtime_error("error"));
              })
              .next([](auto& k) {
                k.Ended();
              })
        | Head();
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          decltype(e())::ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::bad_alloc, std::exception>>);

  EXPECT_THROW_WHAT(*e(), "error");
}
