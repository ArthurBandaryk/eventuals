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

TEST(CatchTest, AllValue) {
  auto e = []() {
    return Just(1)
        | Raise(1)
        | Catch()
              .raised<double>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'double'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              })
              .all([](auto&&...) {
                return 10;
              });
  };

  EXPECT_EQ(10, *e());
}

// TEST(CatchTest, AllEventual) {
//   auto e = []() {
//     return Just(1)
//         | Raise(1)
//         | Catch()
//               .raised<double>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               .raised<std::exception>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               .all([](auto&&...) {
//                 return Just(10);
//               });
//   };

//   EXPECT_EQ(10, *e());
// }

TEST(CatchTest, NoExactHandler) {
  auto e = []() {
    return Just(1)
        | Raise(1)
        | Catch()
              .raised<double>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'double'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              });
  };

  EXPECT_THROW(*e(), int);
}

TEST(CatchTest, AllWithArgument) {
  auto checker = [](int error) {
    EXPECT_EQ(error, 1);
  };

  auto e = [&]() {
    return Just(1)
        | Raise(1)
        | Catch()
              .raised<double>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'double'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              })
              .all([&](auto&&... error) {
                checker(error...);
                return 10;
              });
  };

  EXPECT_EQ(*e(), 10);
}

TEST(CatchTest, ExceptionPtrAllValue) {
  auto expected = []() -> Expected::Of<int> {
    return Unexpected(10);
  };

  auto checker = [](auto error) {
    try {
      std::rethrow_exception(error);
    } catch (int x) {
      EXPECT_EQ(x, 10);
    } catch (...) {
      FAIL() << "Failure on rethrowing";
    }
  };

  auto e = [&]() {
    return expected()
        | Catch()
              .raised<double>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'double'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              })
              // Receive 'std::exception_ptr' there.
              .all([&](auto&&... error) {
                checker(error...);
                return 10;
              });
  };

  EXPECT_EQ(*e(), 10);
}

TEST(CatchTest, ExceptionPtrAllEmpty) {
  auto expected = []() -> Expected::Of<int> {
    return Unexpected(10);
  };

  auto checker = [](auto error) {
    try {
      std::rethrow_exception(error);
    } catch (int x) {
      EXPECT_EQ(x, 10);
    } catch (...) {
      FAIL() << "Failure on rethrowing";
    }
  };

  auto e = [&]() {
    return expected()
        | Catch()
              .raised<double>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'double'";
              })
              .raised<std::exception>([](auto&& error) {
                FAIL() << "Encountered an unexpected raised 'std::exception'";
              })
              // Receive 'std::exception_ptr' there.
              .all([&](auto&&... error) {
                checker(error...);
              });
  };

  EXPECT_THROW(*e(), std::runtime_error);
}

// TEST(CatchTest, ExceptionPtrAllEventual) {
//   auto expected = []() -> Expected::Of<int> {
//     return Unexpected(10);
//   };

//   auto checker = [](auto error) {
//     try {
//       std::rethrow_exception(error);
//     } catch (int x) {
//       EXPECT_EQ(x, 10);
//     } catch (...) {
//       FAIL() << "Failure on rethrowing";
//     }
//   };

//   auto e = [&]() {
//     return expected()
//         | Catch()
//               .raised<double>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'double'";
//               })
//               .raised<std::exception>([](auto&& error) {
//                 FAIL() <<
//"Encountered an unexpected raised 'std::exception'";
//               })
//               // Receive 'std::exception_ptr' there.
//               .all([&](auto&&... error) {
//                 checker(error...);
//                 return Just(10);
//               });
//   };

//   EXPECT_EQ(10, *e());
// }

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
              .raised<Error>([](auto&& error) {
                EXPECT_EQ(std::string("child exception"), error.what());
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
              .all([](auto&&... error) {})
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
              .all([](auto&&...) {
                return 10;
              });
  };

  EXPECT_EQ(10, *e());
}
