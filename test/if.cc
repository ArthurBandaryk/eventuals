#include "eventuals/if.h"

#include <thread>

#include "eventuals/just.h"
#include "eventuals/raise.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/expect-throw-what.h"

using std::string;

using eventuals::Eventual;
using eventuals::If;
using eventuals::Interrupt;
using eventuals::Just;
using eventuals::Raise;
using eventuals::Terminate;
using eventuals::Then;

using testing::MockFunction;

TEST(IfTest, Yes) {
  auto e = []() {
    return Just(1)
        | Then([](int i) {
             return If(i == 1)
                 .yes(Just("yes"))
                 .no(Just("no"));
           });
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          typename decltype(e())::template ErrorsFrom<void, std::tuple<>>,
          std::tuple<>>);

  EXPECT_EQ("yes", *e());
}


TEST(IfTest, No) {
  auto e = []() {
    return Just(0)
        | Then([](int i) {
             return If(i == 1)
                 .yes(Just("yes"))
                 .no(Just("no"));
           });
  };

  EXPECT_EQ("no", *e());
}


TEST(IfTest, Fail) {
  auto e = []() {
    return Just(0)
        | Raise("error")
        | Then([](int i) {
             return If(i == 1)
                 .yes(Just("yes"))
                 .no(Just("no"));
           });
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          typename decltype(e())::template ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  EXPECT_THROW_WHAT(*e(), "error");
}


TEST(IfTest, Interrupt) {
  // Using mocks to ensure start is only called once.
  MockFunction<void()> start;

  auto e = [&]() {
    return Just(1)
        | Then([&](int i) {
             return If(i == 1)
                 .yes(Eventual<const char*>()
                          .interruptible()
                          .start([&](auto& k, Interrupt::Handler& handler) {
                            handler.Install([&k]() {
                              k.Stop();
                            });
                            start.Call();
                          }))
                 .no(Just("no"));
           });
  };

  auto [future, k] = Terminate(e());

  Interrupt interrupt;

  k.Register(interrupt);

  EXPECT_CALL(start, Call())
      .WillOnce([&]() {
        interrupt.Trigger();
      });

  k.Start();

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}


TEST(IfTest, Raise) {
  auto e = []() {
    return Just(1)
        | Then([](int i) {
             return If(i == 1)
                 .yes(Just(i))
                 .no(Raise("raise"));
           });
  };

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          typename decltype(e())::template ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  EXPECT_EQ(1, *e());
}
