#include "eventuals/scheduler.h"

#include "glog/logging.h" // For GetTID().

////////////////////////////////////////////////////////////////////////

// Forward declaration.
//
// TODO(benh): don't use private functions from glog; currently we're
// doing this to match glog message thread id's but this is prone to
// breaking if glog makes any internal changes.
#ifdef _WIN32
using pid_t = int;
#endif

namespace google {
namespace glog_internal_namespace_ {

pid_t GetTID();

} // namespace glog_internal_namespace_
} // namespace google

////////////////////////////////////////////////////////////////////////

static auto GetTID() {
  return static_cast<unsigned int>(google::glog_internal_namespace_::GetTID());
}

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

class DefaultScheduler final : public Scheduler {
 public:
  ~DefaultScheduler() override = default;

  bool Continuable(Context*) override {
    return Context::Get()->scheduler() == this;
  }

  void Submit(Callback<void()> callback, Context* context) override {
    // Default scheduler does not defer because it can't (unless we
    // update all calls that "wait" on tasks to execute outstanding
    // callbacks).
    Context* previous = Context::Switch(context);

    EVENTUALS_LOG(1)
        << "'" << context->name() << "' preempted '" << previous->name() << "'";

    callback();

    CHECK_EQ(context, Context::Get());

    Context::Switch(previous);
  }

  void Clone(Context* context) override {
    // This function is intentionally empty because the 'DefaultScheduler'
    // just invokes what ever callback was specified to 'Submit()'.
  }
};

////////////////////////////////////////////////////////////////////////

Scheduler* Scheduler::Default() {
  static Scheduler* scheduler = new DefaultScheduler();
  return scheduler;
}

////////////////////////////////////////////////////////////////////////

static thread_local Scheduler::Context context(
    Scheduler::Default(),
    "[" + std::to_string(static_cast<unsigned int>(GetTID())) + "]");

////////////////////////////////////////////////////////////////////////

thread_local Scheduler::Context* Scheduler::Context::current_ = &context;

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
