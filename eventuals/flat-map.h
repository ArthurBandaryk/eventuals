#pragma once

#include <optional>

#include "eventuals/stream.h"
#include "eventuals/terminal.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {
namespace detail {

////////////////////////////////////////////////////////////////////////

struct _FlatMap {
  template <typename FlatMap_>
  struct Adaptor {
    void Begin(detail::TypeErasedStream& stream) {
      CHECK(streamforeach_->adapted_.has_value());
      CHECK(streamforeach_->inner_ == nullptr);
      streamforeach_->inner_ = &stream;
      streamforeach_->inner_->Next();
    }

    template <typename... Args>
    void Body(Args&&... args) {
      streamforeach_->k_.Body(std::forward<Args>(args)...);
    }

    void Ended() {
      CHECK(streamforeach_->adapted_.has_value());
      streamforeach_->adapted_.reset();
      CHECK(streamforeach_->inner_ != nullptr);
      streamforeach_->inner_ = nullptr;

      if (streamforeach_->done_) {
        streamforeach_->outer_->Done();
      } else {
        streamforeach_->outer_->Next();
      }
    }

    void Stop() {
      streamforeach_->Stop();
    }

    void Register(Interrupt&) {
      // Already registered K once in 'FlatMap::Body()'.
    }

    FlatMap_* streamforeach_;
  };

  template <typename K_, typename F_, typename Arg_>
  struct Continuation : public detail::TypeErasedStream {
    // NOTE: explicit constructor because inheriting 'TypeErasedStream'.
    Continuation(K_ k, F_ f)
      : k_(std::move(k)),
        f_(std::move(f)) {}

    void Begin(detail::TypeErasedStream& stream) {
      outer_ = &stream;
      previous_ = Scheduler::Context::Get();

      k_.Begin(*this);
    }

    template <typename... Args>
    void Fail(Args&&... args) {
      k_.Fail(std::forward<Args>(args)...);
    }

    void Stop() {
      done_ = true;
      k_.Stop();
    }

    void Register(Interrupt& interrupt) {
      assert(interrupt_ == nullptr);
      interrupt_ = &interrupt;
      k_.Register(interrupt);
    }

    template <typename... Args>
    void Body(Args&&... args) {
      CHECK(!adapted_.has_value());

      adapted_.emplace(
          f_(std::forward<Args>(args)...)
              .template k<void>(Adaptor<Continuation>{this}));

      if (interrupt_ != nullptr) {
        adapted_->Register(*interrupt_);
      }

      adapted_->Start();
    }

    void Ended() {
      CHECK(!adapted_.has_value());
      k_.Ended();
    }

    void Next() override {
      previous_->Continue([this]() {
        if (adapted_.has_value()) {
          CHECK_NOTNULL(inner_)->Next();
        } else {
          outer_->Next();
        }
      });
    }

    void Done() override {
      previous_->Continue([this]() {
        done_ = true;
        if (adapted_.has_value()) {
          CHECK_NOTNULL(inner_)->Done();
        } else {
          outer_->Done();
        }
      });
    }

    K_ k_;
    F_ f_;

    detail::TypeErasedStream* outer_ = nullptr;
    detail::TypeErasedStream* inner_ = nullptr;

    using E_ = typename std::invoke_result<F_, Arg_>::type;

    using Adapted_ = decltype(std::declval<E_>().template k<void>(
        std::declval<Adaptor<Continuation>>()));

    std::optional<Adapted_> adapted_;

    Interrupt* interrupt_ = nullptr;

    bool done_ = false;

    Scheduler::Context* previous_ = nullptr;
  };

  template <typename F_>
  struct Composable {
    template <typename Arg>
    using ValueFrom = typename std::conditional_t<
        std::is_void_v<Arg>,
        std::invoke_result<F_>,
        std::invoke_result<F_, Arg>>::type::template ValueFrom<void>;

    template <typename Arg, typename K>
    auto k(K k) && {
      return Continuation<K, F_, Arg>{std::move(k), std::move(f_)};
    }

    F_ f_;
  };
};

////////////////////////////////////////////////////////////////////////

} // namespace detail

////////////////////////////////////////////////////////////////////////

template <typename F>
auto FlatMap(F f) {
  return detail::_FlatMap::Composable<F>{std::move(f)};
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////