#pragma once

#include "eventuals/eventual.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

struct _Raise final {
  template <typename K_, typename T_>
  struct Continuation final {
    Continuation(K_ k, T_ t)
      : t_(std::move(t)),
        k_(std::move(k)) {}

    template <typename... Args>
    void Start(Args&&...) {
      k_.Fail(std::move(t_));
    }

    template <typename Error>
    void Fail(Error&& error) {
      k_.Fail(std::forward<Error>(error));
    }

    void Stop() {
      k_.Stop();
    }

    void Register(Interrupt& interrupt) {
      k_.Register(interrupt);
    }

    T_ t_;

    // NOTE: we store 'k_' as the _last_ member so it will be
    // destructed _first_ and thus we won't have any use-after-delete
    // issues during destruction of 'k_' if it holds any references or
    // pointers to any (or within any) of the above members.
    K_ k_;
  };

  template <typename T_>
  struct Composable final {
    template <typename Arg>
    using ValueFrom = Arg;

    template <typename Arg, typename Errors>
    using ErrorsFrom = tuple_types_union_t<std::tuple<T_>, Errors>;

    template <typename Arg, typename K>
    auto k(K k) && {
      return Continuation<K, T_>{std::move(k), std::move(t_)};
    }

    T_ t_;
  };
};

////////////////////////////////////////////////////////////////////////

template <typename T>
auto Raise(T t) {
  return _Raise::Composable<T>{std::move(t)};
}

////////////////////////////////////////////////////////////////////////

inline auto Raise(const std::string& s) {
  return Raise(std::runtime_error(s));
}

////////////////////////////////////////////////////////////////////////

inline auto Raise(char* s) {
  return Raise(std::runtime_error(s));
}

////////////////////////////////////////////////////////////////////////

inline auto Raise(const char* s) {
  return Raise(std::runtime_error(s));
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
