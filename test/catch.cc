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

// struct ReturnVoidType {};
// struct ReturnValueType {};
// struct ReturnEventualType {};

// template <typename Type>
// class CatchTypedTest : public testing::Test {
//  public:
//   template <
//       bool RaisedHandler,
//       typename ReturnValue,
//       typename ReturnEventual,
//       typename F>
//   auto GetCatchHandler(
//       ReturnValue&& return_value,
//       ReturnEventual&& return_eventual,
//       F&& f) {
//     if constexpr (std::is_same_v<Type, ReturnVoidType>) {
//       if constexpr (RaisedHandler) {
//         return [f = std::move(f)](auto&& error) {
//           f(error);
//         };
//       } else {
//         return [f = std::move(f)](auto&&... error) {
//           f(error...);
//         };
//       }
//     } else if constexpr (std::is_same_v<Type, ReturnValueType>) {
//       if constexpr (RaisedHandler) {
//         return [return_value = std::move(return_value),
//                 f = std::move(f)](auto&& error) {
//           f(error);
//           return return_value;
//         };
//       } else {
//         return [return_value = std::move(return_value),
//                 f = std::move(f)](auto&&... error) {
//           f(error...);
//           return return_value;
//         };
//       }
//     } else {
//       if constexpr (RaisedHandler) {
//         return [return_eventual = std::move(return_eventual),
//                 f = std::move(f)](auto&& error) {
//           f(error);
//           return return_eventual;
//         };
//       } else {
//         return [return_eventual = std::move(return_eventual),
//                 f = std::move(f)](auto&&... error) {
//           f(error...);
//           return return_eventual;
//         };
//       }
//     }
//   }
// };

// using CatchTypes = testing::Types<
//     ReturnVoidType,
//     ReturnValueType,
//     ReturnEventualType>;

// TYPED_TEST_SUITE(CatchTypedTest, CatchTypes);

// TYPED_TEST(CatchTypedTest, RaisedRuntimeError) {
//   auto e = [&]() {
//     return Just(1)
//         | Raise(std::runtime_error("message"))
//         | Catch()
//               .raised<int>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               .template raised<std::runtime_error>(
//                   this->template GetCatchHandler<true>(
//                       100,
//                       Just(100),
//                       [](auto&& error) {
//                         EXPECT_EQ(std::string("message"), error.what());
//                       }));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
// }

// TYPED_TEST(CatchTypedTest, ChildException) {
//   struct Error : public std::exception {
//     const char* what() const noexcept override {
//       return "child exception";
//     }
//   };

//   auto e = [&]() {
//     return Just(1)
//         | Raise(Error{})
//         | Catch()
//               .raised<int>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               .template raised<std::exception>(
//                   this->template GetCatchHandler<true>(
//                       100,
//                       Just(100),
//                       [](auto&& error) {
//                         EXPECT_EQ(
//                             std::string("child exception"),
//                             error.what());
//                       }));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
// }

// TYPED_TEST(CatchTypedTest, All) {
//   auto e = [&]() {
//     return Just(1)
//         | Raise(1)
//         | Catch()
//               .raised<double>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'double'";
//               })
//               .template raised<std::exception>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'std::exception'";
//               })
//               .all(this->template GetCatchHandler<false>(
//                   100,
//                   Just(100),
//                   [](auto&& error) {
//                     EXPECT_EQ(error, 1);
//                   }));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
// }

// TYPED_TEST(CatchTypedTest, UnexpectedRaise) {
//   struct Error : public std::exception {
//     const char* what() const noexcept override {
//       return "child exception";
//     }
//   };

//   auto expected = []() -> Expected::Of<int> {
//     return Unexpected(Error{});
//   };

//   auto e = [&]() {
//     return expected() // Throwing 'std::exception_ptr' there.
//         | Catch()
//               .raised<int>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               // Receive 'Error' type there, that had been rethrowed from
//               // 'std::exception_ptr'.
//               .template raised<Error>(this->template GetCatchHandler<true>(
//                   100,
//                   Just(100),
//                   [](auto&& error) {
//                     EXPECT_EQ(std::string("child exception"), error.what());
//                   }));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
// }

// TYPED_TEST(CatchTypedTest, UnexpectedAll) {
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
//     return expected() // Throwing 'std::exception_ptr' there.
//         | Catch()
//               .raised<double>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'double'";
//               })
//               .template raised<std::exception>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'std::exception'";
//               })
//               // Receive 'std::exception_ptr' there.
//               .all(this->template GetCatchHandler<false>(
//                   100,
//                   Just(100),
//                   std::move(checker)));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
// }

// TYPED_TEST(CatchTypedTest, FailNoArgs) {
//   auto e = [&]() {
//     return Just(1)
//         | Eventual<int>([](auto& k, auto&& value) {
//              // Fail with no arguments.
//              k.Fail();
//            })
//         | Catch()
//               .raised<int>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'int'";
//               })
//               .template raised<std::exception>([](auto&& error) {
//                 FAIL() << "Encountered an unexpected raised 'std::exception'";
//               })
//               .all(this->template GetCatchHandler<false>(
//                   100,
//                   Just(100),
//                   [](auto&&... errors) {
//                     static_assert(sizeof...(errors) == 0);
//                   }));
//   };

//   if constexpr (std::is_same_v<TypeParam, ReturnVoidType>) {
//     EXPECT_THROW(*e(), std::runtime_error);
//   } else {
//     EXPECT_EQ(*e(), 100);
//   }
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

TEST(CatchTest, ReRaise) {
  auto e = [&]() {
    return Just(1)
        | Raise(1)
        | Catch()
              .raised<int>([](auto&& error) {
                return Raise(1);
              })
              .raised<std::exception_ptr>([](auto&& error) {})
              .all([](auto&&... error) {
                return Just(11);
              })
        | Catch()
              .raised<int>([](auto&& error) {
                EXPECT_EQ(error, 1);
                return Just(10);
              });
  };

  EXPECT_EQ(10, *e());
}
