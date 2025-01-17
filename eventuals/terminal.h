#pragma once

#include <future>
#include <optional>

#include "eventuals/eventual.h"
#include "eventuals/interrupt.h"
#include "eventuals/undefined.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

struct _Terminal final {
  template <
      typename Context_,
      typename Start_,
      typename Fail_,
      typename Stop_>
  struct Continuation final {
    template <typename... Args>
    void Start(Args&&... args) {
      if constexpr (IsUndefined<Start_>::value) {
        EVENTUALS_LOG(1)
            << "'Terminal::Start()' reached by "
            << Scheduler::Context::Get()->name()
            << " but undefined";
      } else {
        if constexpr (IsUndefined<Context_>::value) {
          start_(std::forward<Args>(args)...);
        } else {
          start_(context_, std::forward<Args>(args)...);
        }
      }
    }

    template <typename Error>
    void Fail(Error&& error) {
      if constexpr (IsUndefined<Fail_>::value) {
        EVENTUALS_LOG(1)
            << "'Terminal::Fail()' reached by "
            << Scheduler::Context::Get()->name()
            << " but undefined";
      } else {
        if constexpr (IsUndefined<Context_>::value) {
          fail_(std::forward<Error>(error));
        } else {
          fail_(context_, std::forward<Error>(error));
        }
      }
    }

    void Stop() {
      if constexpr (IsUndefined<Stop_>::value) {
        EVENTUALS_LOG(1)
            << "'Terminal::Stop()' reached by "
            << Scheduler::Context::Get()->name()
            << " but undefined";
      } else {
        if constexpr (IsUndefined<Context_>::value) {
          stop_();
        } else {
          stop_(context_);
        }
      }
    }

    void Register(Interrupt&) {}

    Context_ context_;
    Start_ start_;
    Fail_ fail_;
    Stop_ stop_;
  };

  template <
      typename Context_,
      typename Start_,
      typename Fail_,
      typename Stop_>
  struct Builder final {
    template <typename...>
    using ValueFrom = void;

    template <typename Arg, typename Errors>
    using ErrorsFrom = Errors;

    template <
        typename Context,
        typename Start,
        typename Fail,
        typename Stop>
    static auto create(
        Context context,
        Start start,
        Fail fail,
        Stop stop) {
      return Builder<
          Context,
          Start,
          Fail,
          Stop>{
          std::move(context),
          std::move(start),
          std::move(fail),
          std::move(stop)};
    }

    template <typename Arg, typename... K>
    auto k(K...) && {
      static_assert(
          sizeof...(K) == 0,
          "detected invalid continuation composed _after_ 'Terminal'");

      return Continuation<
          Context_,
          Start_,
          Fail_,
          Stop_>{
          std::move(context_),
          std::move(start_),
          std::move(fail_),
          std::move(stop_)};
    }

    template <typename Context>
    auto context(Context context) && {
      static_assert(IsUndefined<Context_>::value, "Duplicate 'context'");
      return create(
          std::move(context),
          std::move(start_),
          std::move(fail_),
          std::move(stop_));
    }

    template <typename Start>
    auto start(Start start) && {
      static_assert(IsUndefined<Start_>::value, "Duplicate 'start'");
      return create(
          std::move(context_),
          std::move(start),
          std::move(fail_),
          std::move(stop_));
    }

    template <typename Fail>
    auto fail(Fail fail) && {
      static_assert(IsUndefined<Fail_>::value, "Duplicate 'fail'");
      return create(
          std::move(context_),
          std::move(start_),
          std::move(fail),
          std::move(stop_));
    }

    template <typename Stop>
    auto stop(Stop stop) && {
      static_assert(IsUndefined<Stop_>::value, "Duplicate 'stop'");
      return create(
          std::move(context_),
          std::move(start_),
          std::move(fail_),
          std::move(stop));
    }


    Context_ context_;
    Start_ start_;
    Fail_ fail_;
    Stop_ stop_;
  };
};

////////////////////////////////////////////////////////////////////////

inline auto Terminal() {
  return _Terminal::Builder<
      Undefined,
      Undefined,
      Undefined,
      Undefined>{};
}

////////////////////////////////////////////////////////////////////////

struct StoppedException final : public std::exception {
  StoppedException() = default;
  StoppedException(const StoppedException& that) = default;
  StoppedException(StoppedException&& that) = default;

  ~StoppedException() override = default;

  const char* what() const throw() override {
    return "Eventual computation stopped (cancelled)";
  }
};

////////////////////////////////////////////////////////////////////////

// Using to don't create nested 'std::exception_ptr'.
template <typename... Args>
auto make_exception_ptr_or_forward(Args&&... args) {
  static_assert(sizeof...(args) > 0, "Expecting an error");
  static_assert(!std::is_same_v<std::decay_t<Args>..., std::exception_ptr>);
  static_assert(
      std::is_base_of_v<std::exception, std::decay_t<Args>...>,
      "Expecting a type derived from std::exception");
  return std::make_exception_ptr(std::forward<Args>(args)...);
}

inline auto make_exception_ptr_or_forward(std::exception_ptr error) {
  return error;
}

////////////////////////////////////////////////////////////////////////

// Using to get right type for 'std::promise' at 'Terminate' because
// using 'std::promise<std::reference_wrapper<T>>' is forbidden on
// Windows build using MSVC.
// https://stackoverflow.com/questions/49830864
template <typename T>
struct ReferenceWrapperTypeExtractor {
  using type = T;
};

template <typename T>
struct ReferenceWrapperTypeExtractor<std::reference_wrapper<T>> {
  using type = T&;
};

////////////////////////////////////////////////////////////////////////

template <typename E>
auto Terminate(E e) {
  using Value = typename E::template ValueFrom<void>;

  std::promise<
      typename ReferenceWrapperTypeExtractor<Value>::type>
      promise;

  auto future = promise.get_future();

  auto k =
      (std::move(e)
       | eventuals::Terminal()
             .context(std::move(promise))
             .start([](auto& promise, auto&&... values) {
               static_assert(
                   sizeof...(values) == 0 || sizeof...(values) == 1,
                   "Task only supports 0 or 1 value, but found > 1");
               promise.set_value(std::forward<decltype(values)>(values)...);
             })
             .fail([](auto& promise, auto&&... errors) {
               static_assert(
                   sizeof...(errors) == 0 || sizeof...(errors) == 1,
                   "Task only supports 0 or 1 error, but found > 1");

               promise.set_exception(
                   make_exception_ptr_or_forward(
                       std::forward<decltype(errors)>(errors)...));
             })
             .stop([](auto& promise) {
               promise.set_exception(
                   std::make_exception_ptr(
                       StoppedException()));
             }))
          .template k<void>();

  return std::make_tuple(std::move(future), std::move(k));
}

////////////////////////////////////////////////////////////////////////

template <typename E>
auto operator*(E e) {
  auto [future, k] = Terminate(std::move(e));

  k.Start();

  try {
    return future.get();
  } catch (const std::exception& e) {
    LOG(WARNING)
        << "WARNING: exception thrown while dereferencing eventual: "
        << e.what();
    throw;
  } catch (...) {
    LOG(WARNING)
        << "WARNING: exception thrown while dereferencing eventual";
    throw;
  }
}

////////////////////////////////////////////////////////////////////////

template <typename Arg, typename E>
auto Build(E e) {
  return std::move(e).template k<Arg>();
}

////////////////////////////////////////////////////////////////////////

template <typename Arg, typename E, typename K>
auto Build(E e, K k) {
  return std::move(e).template k<Arg>(std::move(k));
}

////////////////////////////////////////////////////////////////////////

template <typename E>
auto Build(E e) {
  return std::move(e).template k<void>();
}

////////////////////////////////////////////////////////////////////////

template <typename E, typename K>
auto Build(E e, K k) {
  return std::move(e).template k<void>(std::move(k));
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
