#include "stout/stream-for-each.h"

#include <chrono>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stout/collect.h"
#include "stout/event-loop.h"
#include "stout/iterate.h"
#include "stout/let.h"
#include "stout/map.h"
#include "stout/range.h"
#include "stout/stream.h"
#include "stout/terminal.h"
#include "stout/then.h"
#include "stout/timer.h"
#include "test/event-loop-test.h"

using stout::eventuals::Collect;
using stout::eventuals::EventLoop;
using stout::eventuals::Interrupt;
using stout::eventuals::Iterate;
using stout::eventuals::Let;
using stout::eventuals::Map;
using stout::eventuals::Range;
using stout::eventuals::Stream;
using stout::eventuals::StreamForEach;
using stout::eventuals::Then;
using stout::eventuals::Timer;
using testing::ElementsAre;

// class ffx : public EventLoopTest {};

// TEST_F(ffx, JustInterrupt) {
//   auto e = []() {
//     return Timer(std::chrono::milliseconds(1))
//         | Iterate(std::vector<int>(10000))
//         | StreamForEach([](int x) { return Range(2); })
//         | Collect<std::vector<int>>()
//               .stop([](auto& data, auto& k) {
//                 k.Start(std::move(data));
//               });
//   };

//   auto [future, k] = Terminate(e());

//   Interrupt interrupt;

//   k.Register(interrupt);

//   k.Start();

//   interrupt.Trigger();

//   EventLoop::Default().Run();

//   EXPECT_THROW(future.get(), stout::eventuals::StoppedException);
// }

// TEST(StreamForEach, TwoLevelLoop) {
//   auto s = []() {
//     return Range(2)
//         | StreamForEach([](int x) { return Range(2); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(0, 1, 0, 1));
// }

// TEST(StreamForEach, TwoIndexesSum) {
//   auto s = []() {
//     return Range(3)
//         | StreamForEach([](int x) {
//              return Stream<int>()
//                  .next([container = std::vector<int>({1, 2}),
//                         i = 0u,
//                         x](auto& k) mutable {
//                    if (i != container.size()) {
//                      k.Emit(container[i++] + x);
//                    } else {
//                      k.Ended();
//                    }
//                  })
//                  .done([](auto& k) {
//                    k.Ended();
//                  });
//            })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(1, 2, 2, 3, 3, 4));
// }

// TEST(StreamForEach, TwoIndexesSumMap) {
//   auto s = []() {
//     return Range(3)
//         | StreamForEach([](int x) { return Range(1, 3)
//                                         | Map(Then([x](int y) { return x + y; })); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(1, 2, 2, 3, 3, 4));
// }

// TEST(StreamForEach, ThreeIndexesSumMap) {
//   auto s = []() {
//     return Range(3)
//         | StreamForEach([](int x) { return Range(1, 3)
//                                         | Map(Then([x](int y) { return x + y; })); })
//         | StreamForEach([](int x) { return Range(1, 3)
//                                         | Map(Then([x](int y) { return x + y; })); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(2, 3, 3, 4, 3, 4, 4, 5, 4, 5, 5, 6));
// }

// TEST(StreamForEach, ThreeLevelLoop) {
//   auto s = []() {
//     return Range(2)
//         | StreamForEach([](int x) { return Range(2); })
//         | StreamForEach([](int x) { return Range(2); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(0, 1, 0, 1, 0, 1, 0, 1));
// }

TEST(StreamForEach, ThreeLevelLoop2) {
  auto s = []() {
    return Range(2)
        | StreamForEach([](int x) { return Range(2) | StreamForEach([](int y) { return Range(2); }); })
        | Collect<std::vector<int>>();
  };

  EXPECT_THAT(*s(), ElementsAre(0, 1, 0, 1, 0, 1, 0, 1));
}

// TEST(StreamForEach, StreamForEachMapped) {
//   auto s = []() {
//     return Range(2)
//         | StreamForEach([](int x) { return Range(2); })
//         | Map(Then([](int x) { return x + 1; }))
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(1, 2, 1, 2));
// }

// TEST(StreamForEach, StreamForEachIterate) {
//   auto s = []() {
//     return Range(2)
//         | StreamForEach([](int x) {
//              std::vector<int> v = {1, 2, 3};
//              return Iterate(std::move(v));
//            })
//         | Map(Then([](int x) { return x + 1; }))
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(2, 3, 4, 2, 3, 4));
// }

// TEST(StreamForEach, StreamForEachIterateString) {
//   auto s = []() {
//     return Iterate(std::vector<std::string>({"abc", "abc"}))
//         | StreamForEach([](std::string x) {
//              std::vector<int> v = {1, 2, 3};
//              return Iterate(std::move(v));
//            })
//         | Map(Then([](int x) { return x + 1; }))
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(2, 3, 4, 2, 3, 4));
// }

// TEST(StreamForEach, VectorVector) {
//   auto s = []() {
//     return Iterate(std::vector<int>({2, 3, 14}))
//         | StreamForEach([](int x) {
//              std::vector<std::vector<int>> c;
//              c.push_back(std::vector<int>());
//              c.push_back(std::vector<int>());
//              return Iterate(std::move(c));
//            })
//         | StreamForEach([](std::vector<int> x) { return Range(2); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1));
// }

// TEST(StreamForEach, Vector) {
//   auto s = []() {
//     return Iterate(std::vector<std::string>({"2", "3", "14"}))
//         | StreamForEach([](std::string x) {
//              return Iterate(std::vector<int>({1, 2}));
//            })
//         | StreamForEach([](int x) { return Range(2); })
//         | Collect<std::vector<int>>();
//   };

//   EXPECT_THAT(*s(), ElementsAre(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1));
// }