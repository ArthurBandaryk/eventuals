#include "eventuals/catch.h"

#include <iostream>
#include <string>

#include "eventuals/conditional.h"
#include "eventuals/eventual.h"
#include "eventuals/expected.h"
#include "eventuals/just.h"
#include "eventuals/raise.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using eventuals::Catch;
using eventuals::Conditional;
using eventuals::Eventual;
using eventuals::Expected;
using eventuals::Just;
using eventuals::Raise;
using eventuals::Then;
using eventuals::Unexpected;
using testing::MockFunction;

TEST(CatchTest, RaisedRuntimeError) {
  auto e = []() {
    return Just(1)
        | Raise(std::runtime_error("message"))
        | Catch()
              .raised<int>([](auto&& error) {
                ADD_FAILURE() << "Encountered an unexpected raised 'int'";
                return Then([]() {
                  return 100;
                });
              })
              .raised<std::runtime_error>([](auto&&) {
                return Just(100);
              });
  };

  EXPECT_EQ(*e(), 100);
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
                ADD_FAILURE() << "Encountered an unexpected raised 'int'";
                return Just(10);
              })
              .raised<std::exception>(
                  [](auto&& error) {
                    EXPECT_STREQ("child exception", error.what());
                    return Just(100);
                  });
  };

  EXPECT_EQ(*e(), 100);
}

TEST(CatchTest, All) {
  auto e = []() {
    return Just(500)
        | Raise(std::runtime_error("10"))
        | Catch()
              .raised<double>([](auto&& error) {
                ADD_FAILURE() << "Encountered an unexpected raised 'double'";
                return 10;
              })
              .raised<std::string>([](auto&& error) {
                ADD_FAILURE()
                    << "Encountered an unexpected raised 'std::string'";
                return 10;
              })
              .all([](std::exception_ptr&& error) {
                try {
                  std::rethrow_exception(error);
                } catch (const std::runtime_error& error) {
                  EXPECT_STREQ(error.what(), "10");
                }
                return 100;
              })
        | Then([](int&& value) {
             return value;
           });
  };

  EXPECT_EQ(*e(), 100);
}

TEST(CatchTest, UnexpectedRaise) {
  struct Error : public std::exception {
    const char* what() const noexcept override {
      return "child exception";
    }
  };

  auto expected = []() -> Expected::Of<int> {
    return Unexpected(Error{});
  };

  auto e = [&]() {
    return expected() // Throwing 'std::exception_ptr' there.
        | Catch()
              .raised<int>([](auto&& error) {
                ADD_FAILURE() << "Encountered an unexpected raised 'int'";
                return 1;
              })
              // Receive 'Error' type there, that had been rethrowed from
              // 'std::exception_ptr'.
              .raised<Error>([](auto&& error) {
                EXPECT_STREQ("child exception", error.what());
                return 100;
              });
  };

  EXPECT_EQ(*e(), 100);
}

TEST(CatchTest, UnexpectedAll) {
  struct Error : public std::exception {
    const char* what() const noexcept override {
      return "child exception";
    }
  };

  auto expected = []() -> Expected::Of<int> {
    return Unexpected(Error{});
  };

  auto e = [&]() {
    return expected() // Throwing 'std::exception_ptr' there.
        | Catch()
              .raised<double>([](auto&& error) {
                ADD_FAILURE() << "Encountered an unexpected raised 'double'";
                return 1;
              })
              .raised<std::string>([](auto&& error) {
                ADD_FAILURE()
                    << "Encountered an unexpected raised 'std::string'";
                return 1;
              })
              .all([](std::exception_ptr&& error) {
                try {
                  std::rethrow_exception(error);
                } catch (const Error& e) {
                  EXPECT_STREQ(e.what(), "child exception");
                } catch (...) {
                  ADD_FAILURE() << "Failure on rethrowing";
                }

                return 100;
              });
  };

  EXPECT_EQ(*e(), 100);
}

TEST(CatchTest, NoExactHandler) {
  auto e = []() {
    return Just(1)
        | Raise(std::string("error"))
        | Catch()
              .raised<double>([](auto&& error) {
                ADD_FAILURE() << "Encountered an unexpected raised 'double'";
                return 1;
              })
              .raised<std::string>([](auto&& error) {
                ADD_FAILURE()
                    << "Encountered an unexpected raised 'std::string'";
                return 1;
              });
  };

  EXPECT_THROW(*e(), std::runtime_error);
}

TEST(CatchTest, ReRaise) {
  auto e = []() {
    return Just(1)
        | Raise("10")
        | Catch()
              .raised<std::runtime_error>([](auto&& error) {
                EXPECT_EQ(error.what(), std::string("10"));
                return Raise("1");
              })
              .all([](std::exception_ptr&& error) {
                ADD_FAILURE() << "Encountered an unexpected all";
                return Just(100);
              })
        | Then([](auto&&) {
             return 200;
           })
        | Catch()
              .raised<std::runtime_error>([](auto&& error) {
                EXPECT_EQ(error.what(), std::string("1"));
                return Just(10);
              })
        | Then([](auto value) {
             return value;
           });
  };

  EXPECT_EQ(10, *e());
}

TEST(CatchTest, VoidPropagate) {
  auto e = []() {
    return Just("error")
        | Then([](const char* i) {
             return;
           })
        | Catch()
              .raised<std::exception>([](auto&& e) {
                // MUST RETURN VOID HERE!
              })
        | Then([](/* MUST TAKE VOID HERE! */) {
             return 100;
           });
  };

  EXPECT_EQ(100, *e());
}
