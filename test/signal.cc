#include "eventuals/signal.h"

#include <thread>

#include "event-loop-test.h"
#include "eventuals/event-loop.h"
#include "eventuals/scheduler.h"
#include "eventuals/terminal.h"
#include "eventuals/then.h"
#include "eventuals/type-traits.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using eventuals::EventLoop;
using eventuals::Eventual;
using eventuals::Interrupt;
using eventuals::Scheduler;
using eventuals::Terminate;
using eventuals::Then;
using eventuals::WaitForOneOfSignals;
using eventuals::WaitForSignal;

using namespace std::chrono_literals;

// Windows notes!
//
// On Windows calls to raise() or abort() to programmatically
// raise a signal are not detected by libuv; these will not
// trigger a signal watcher. The link below will be helpful!
// http://docs.libuv.org/en/v1.x/signal.html?highlight=uv_signal_t#c.uv_signal_t

// TODO: think later about possible way of raising signals on Windows.

class SignalTest : public EventLoopTest {};

#if !defined(_WIN32)

TEST_F(SignalTest, SignalComposition) {
  auto e = WaitForSignal(SIGQUIT)
      | Then([]() {
             return "quit";
           });

  static_assert(
      eventuals::tuple_types_unordered_equals_v<
          typename decltype(e)::template ErrorsFrom<void, std::tuple<>>,
          std::tuple<std::runtime_error>>);

  auto [future, k] = Terminate(std::move(e));

  k.Start();

  // NOTE: now that we've started the continuation 'k' we will have
  // submitted a callback to the event loop and thus by explicitly
  // submitting another callback we will ensure there is a
  // happens-before relationship between setting up the signal handler
  // and raising the signal.
  Scheduler::Context context(&EventLoop::Default(), "raise(SIGQUIT)");

  EventLoop::Default().Submit(
      []() {
        EXPECT_EQ(raise(SIGQUIT), 0);
      },
      &context);

  EventLoop::Default().RunUntil(future);

  EXPECT_EQ(future.get(), "quit");
}

TEST_F(SignalTest, WaitForSignal) {
  auto e = WaitForOneOfSignals({SIGQUIT});

  auto [future, k] = Terminate(std::move(e));

  k.Start();

  // NOTE: now that we've started the continuation 'k' we will have
  // submitted a callback to the event loop and thus by explicitly
  // submitting another callback we will ensure there is a
  // happens-before relationship between setting up the signal handler
  // and raising the signal.
  Scheduler::Context context(&EventLoop::Default(), "raise(SIGQUIT)");

  EventLoop::Default().Submit(
      []() {
        EXPECT_EQ(raise(SIGQUIT), 0);
      },
      &context);

  EventLoop::Default().RunUntil(future);

  EXPECT_EQ(future.get(), SIGQUIT);
}

#endif // !defined(_WIN32)


TEST_F(SignalTest, SignalInterrupt) {
  auto [future, k] = Terminate(WaitForSignal(SIGINT));

  Interrupt interrupt;

  k.Register(interrupt);

  k.Start();

  interrupt.Trigger();

  EventLoop::Default().RunUntil(future);

  EXPECT_THROW(future.get(), eventuals::StoppedException);
}
