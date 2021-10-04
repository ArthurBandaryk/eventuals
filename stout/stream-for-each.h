#pragma once

#include <optional>

#include "stout/stream.h"
#include "stout/terminal.h"

////////////////////////////////////////////////////////////////////////

namespace stout {
namespace eventuals {
namespace detail {

////////////////////////////////////////////////////////////////////////

struct _StreamForEach {
  template <typename StreamForEach_>
  struct Adaptor {
    void Start(detail::TypeErasedStream& stream) {
      stream.Next();
    }

    template <typename... Args>
    void Body(Args&&... args) {
      streamforeach_->k_.Body(std::forward<Args>(args)...);
    }

    void Ended() {
      streamforeach_->inner_.reset();

      if (streamforeach_->done_) {
        streamforeach_->outer_->Done();
      } else {
        streamforeach_->outer_->Next();
      }
    }

    void Register(Interrupt&) {
      // Already registered K once in 'Then::Register()'.
    }

    StreamForEach_* streamforeach_;
  };

  template <typename K_, typename F_, typename Arg_>
  struct Continuation : public detail::TypeErasedStream {
    // NOTE: explicit constructor because inheriting 'TypeErasedStream'.
    Continuation(K_ k, F_ f)
      : k_(std::move(k)),
        f_(std::move(f)) {}

    void Start(detail::TypeErasedStream& stream) {
      outer_ = &stream;
      k_.Start(*this);
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

    // can be passed there from inner StreamForEach
    // need to check the previous and pass further if needed
    template <typename... Args>
    void Body(Args&&... args) {
      CHECK(!inner_.has_value());

      inner_.emplace(
          f_(std::forward<Args>(args)...)
              .template k<void>(Adaptor<Continuation>{this}));

      inner_->Start();
    }

    void Ended() {
      CHECK(!inner_.has_value());
      k_.Ended();
    }

    void Next() override {
      if (inner_.has_value()) {
        inner_->Next();
      } else {
        outer_->Next();
      }
    }

    void Done() override {
      done_ = true;
      if (inner_.has_value()) {
        inner_->Done();
      } else {
        outer_->Done();
      }
    }

    K_ k_;
    F_ f_;

    detail::TypeErasedStream* outer_ = nullptr;

    using E_ = typename std::invoke_result<F_, Arg_>::type;

    using Inner_ = decltype(std::declval<E_>().template k<void>(
        std::declval<Adaptor<Continuation>>()));

    std::optional<Inner_> inner_;

    Interrupt* interrupt_ = nullptr;

    bool done_ = false;
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
auto StreamForEach(F f) {
  return detail::_StreamForEach::Composable<F>{std::move(f)};
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals
} // namespace stout

////////////////////////////////////////////////////////////////////////