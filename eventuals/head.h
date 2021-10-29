#pragma once

#include "eventuals/stream.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

namespace detail {

////////////////////////////////////////////////////////////////////////

struct _Head {
  template <typename K_, typename Arg_>
  struct Continuation {
    void Begin(TypeErasedStream& stream) {
      stream_ = &stream;
      stream.Next();
    }

    template <typename... Args>
    void Fail(Args&&... args) {
      k_.Fail(std::forward<Args>(args)...);
    }

    void Stop() {
      k_.Stop();
    }

    void Body(Arg_ arg) {
      arg_.emplace(std::move(arg));
      CHECK_NOTNULL(stream_)->Done();
    }

    void Ended() {
      if (arg_) {
        k_.Start(std::move(*arg_));
      } else {
        k_.Fail("empty stream");
      }
    }

    void Register(Interrupt& interrupt) {
      k_.Register(interrupt);
    }

    K_ k_;

    std::optional<Arg_> arg_;

    TypeErasedStream* stream_ = nullptr;
  };

  struct Composable {
    template <typename Arg>
    using ValueFrom = Arg;

    template <typename Arg, typename K>
    auto k(K k) && {
      return Continuation<K, Arg>{std::move(k)};
    }
  };
};

////////////////////////////////////////////////////////////////////////

} // namespace detail

////////////////////////////////////////////////////////////////////////

inline auto Head() {
  return detail::_Head::Composable{};
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
