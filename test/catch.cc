#include "eventuals/catch.h"

#include <iostream>
#include <string>

#include "eventuals/eventual.h"
#include "eventuals/expected.h"
#include "eventuals/just.h"
#include "eventuals/raise.h"
#include "eventuals/terminal.h"
#include "gtest/gtest.h"

using eventuals::Catch;
using eventuals::Eventual;
using eventuals::Expected;
using eventuals::Just;
using eventuals::Raise;
using eventuals::Unexpected;

TEST(CatchTest, RuntimeErrorValue) {
  auto e = []() {
    return Just(1)
        | Raise(std::runtime_error("message"))
        | Catch()
              .raised<int>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'int'";
              })
              .raised<std::runtime_error>([](auto&& error) {
                EXPECT_EQ(std::string("message"), error.what());
                return 10;
              });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, RuntimeErrorEventual) {
  auto e = []() {
    return Just(1)
        | Raise(std::runtime_error("message"))
        | Catch()
              .raised<int>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'int'";
              })
              .raised<std::runtime_error>([](auto&& error) {
                EXPECT_EQ(std::string("message"), error.what());
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, ChildException) {
  struct Error : public std::exception {
    const char* what() const noexcept override {
      return "child exception";
    }
  };

  auto e = []() {
    return Just(1)
        | Raise(Error{})
        | Catch()
              .raised<int>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'int'";
              })
              .raised<std::exception>([](auto&& error) {
                EXPECT_EQ("child exception", error.what());
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, Expected) {
  struct Error : public std::exception {
    const char* what() const noexcept override {
      return "child exception";
    }
  };

  auto expected = []() -> Expected::Of<int> {
    return Unexpected(Error{});
  };

  auto e = [&]() {
    return expected()
        | Catch()
              .raised<int>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'int'";
              })
              .raised<std::exception_ptr>([](auto&& error) {
                //EXPECT_EQ("child exception", error.what());
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, ReRaise) {
  auto e = [&]() {
    return Just(1)
        | Raise(1)
        | Catch()
              .raised<int>([](auto&& error) {
                return Raise(1);
              })
              .raised<std::exception_ptr>([](auto&& error) {})
              .all([](auto&& error) {})
        | Catch()
              .raised<int>([](auto&& error) {
                EXPECT_EQ(error, 1);
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, NoError) {
  auto e = [&]() {
    return Just(1)
        | Eventual<int>([](auto& k, auto&& value) {
             // Fail with no arguments.
             k.Fail();
           })
        | Catch()
              .raised<int>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'int'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              })
              .all([]() {
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}
