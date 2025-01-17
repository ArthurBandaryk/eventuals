#include "eventuals/finally.h"

#include "eventuals/eventual.h"
#include "eventuals/just.h"
#include "eventuals/raise.h"
#include "eventuals/terminal.h"
#include "gtest/gtest.h"
#include "test/expect-throw-what.h"

using eventuals::Eventual;
using eventuals::Expected;
using eventuals::Finally;
using eventuals::Just;
using eventuals::Raise;

TEST(Finally, Succeed) {
  auto e = []() {
    return Just(42)
        | Finally([](auto&& expected) {
             return Just(std::move(expected));
           });
  };

  auto expected = *e();

  static_assert(std::is_same_v<Expected::Of<int>, decltype(expected)>);

  ASSERT_TRUE(expected);

  EXPECT_EQ(42, *expected);
}

TEST(Finally, Fail) {
  auto e = []() {
    return Just(42)
        | Raise("error")
        | Finally([](auto&& expected) {
             return Just(std::move(expected));
           });
  };

  auto expected = *e();

  static_assert(std::is_same_v<Expected::Of<int>, decltype(expected)>);

  EXPECT_THROW_WHAT(*expected, "error");
}

TEST(Finally, Stop) {
  auto e = []() {
    return Eventual<std::string>([](auto& k) {
             k.Stop();
           })
        | Finally([](auto&& expected) {
             return Just(std::move(expected));
           });
  };

  auto expected = *e();

  static_assert(std::is_same_v<Expected::Of<std::string>, decltype(expected)>);

  EXPECT_THROW(*expected, eventuals::StoppedException);
}

TEST(Finally, VoidSucceed) {
  auto e = []() {
    return Just()
        | Finally([](auto&& exception) {
             return Just(std::move(exception));
           });
  };

  auto exception = *e();

  static_assert(
      std::is_same_v<
          std::optional<std::exception_ptr>,
          decltype(exception)>);

  EXPECT_FALSE(exception.has_value());
}

TEST(Finally, VoidFail) {
  auto e = []() {
    return Just()
        | Raise("error")
        | Finally([](auto&& exception) {
             return Just(std::move(exception));
           });
  };

  auto exception = *e();

  static_assert(
      std::is_same_v<
          std::optional<std::exception_ptr>,
          decltype(exception)>);

  ASSERT_TRUE(exception.has_value());

  EXPECT_THROW_WHAT(
      std::rethrow_exception(exception.value()),
      "error");
}

TEST(Finally, VoidStop) {
  auto e = []() {
    return Eventual<void>([](auto& k) {
             k.Stop();
           })
        | Finally([](auto&& exception) {
             return Just(std::move(exception));
           });
  };

  auto exception = *e();

  static_assert(
      std::is_same_v<
          std::optional<std::exception_ptr>,
          decltype(exception)>);

  ASSERT_TRUE(exception.has_value());

  EXPECT_THROW(
      std::rethrow_exception(exception.value()),
      eventuals::StoppedException);
}
