#pragma once

#include <memory> // For 'std::unique_ptr'.
#include <optional>
#include <tuple>

#include "eventuals/eventual.h"
#include "eventuals/then.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

struct _Catch {
  template <typename Error_, typename Handler_>
  struct Handler {
    using error_type = Error_;

    template <typename E>
    static constexpr bool Handles =
        std::is_same_v<Error_, E> || std::is_base_of_v<Error_, E>;

    Handler_ handler_;
  };

  ////////////////////////////////////////////////////////////////////////

  template <typename K_, bool has_all_, typename... CatchHandlers_>
  struct Continuation {
    template <typename F, typename... Args>
    void BuildAndInvoke(F&& f, Args&&... args) {
      using HandlerInvokeResult =
          typename std::invoke_result<F, Args...>::type;

      if constexpr (HasValueFrom<HandlerInvokeResult>::value) {
        auto k = (f(std::forward<Args>(args)...)
                      .template k<void>(_Then::Adaptor<K_>{k_}));

        if (interrupt_ != nullptr) {
          k.Register(*interrupt_);
        }

        k.Start();
      } else {
        k_.Start(f(std::forward<Args>(args)...));
      }
    }

    template <
        typename Error_,
        typename CatchHandler_,
        typename... Tail>
    void UnpackInvokeHelper(
        Error_&& error,
        CatchHandler_&& handler,
        Tail&&... tail) {
      if constexpr (CatchHandler_::template Handles<Error_>) {
        BuildAndInvoke(std::move(handler.handler_), std::move(error));
      } else {
        UnpackInvokeHelper<Error_>(
            std::move(error),
            std::forward<Tail>(tail)...);
      }
    }

    template <typename Error_, typename CatchHandler_>
    void UnpackInvokeHelper(
        Error_&& error,
        CatchHandler_&& handler) {
      if constexpr (CatchHandler_::template Handles<Error_>) {
        BuildAndInvoke(std::move(handler.handler_), std::move(error));
      } else if constexpr (has_all_) {
        // Last handler should be always 'all' handler.
        BuildAndInvoke(
            std::move(
                std::get<
                    std::tuple_size<
                        decltype(catch_handlers_)>::value
                    - 1>(catch_handlers_)
                    .handler_),
            std::move(error));
      }
    }

    template <typename... Args>
    void Start(Args&&... args) {
      k_.Start(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Fail(Args&&... args) {
      // propagate k.Fail otherwise?
      static_assert(
          sizeof...(args) == 0 || sizeof...(args) == 1,
          "Catch only supports 0 or 1 argument, but found > 1");

      if constexpr (sizeof...(args) > 0) {
        std::apply(
            [&](auto&&... catch_handlers) {
              UnpackInvokeHelper(
                  std::forward<decltype(args)>(args)...,
                  std::forward<decltype(catch_handlers)>(catch_handlers)...);
            },
            std::move(catch_handlers_));
      } else {
        // Last handler should be always 'all' handler.
        if constexpr (has_all_) {
          BuildAndInvoke(std::move(
              std::get<
                  std::tuple_size<
                      decltype(catch_handlers_)>::value
                  - 1>(catch_handlers_)
                  .handler_));
        } else {
          k_.Fail(std::forward<decltype(args)>(args)...);
        }
      }
    }

    void Stop() {
      k_.Stop();
    }

    void Register(Interrupt& interrupt) {
      interrupt_ = &interrupt;
      k_.Register(interrupt);
    }

    K_ k_;
    std::tuple<CatchHandlers_...> catch_handlers_;

    Interrupt* interrupt_ = nullptr;
  };

  template <bool has_all_ = false, typename... CatchHandlers_>
  struct Builder {
    template <typename Arg>
    using ValueFrom = Arg;

    template <typename Arg, typename K>
    auto k(K k) && {
      static_assert(
          sizeof...(CatchHandlers_) > 0 || has_all_,
          "No handlers were specified for 'Catch'");

      return Continuation<K, has_all_, CatchHandlers_...>{
          std::move(k),
          std::move(catch_handlers_)};
    }

    template <typename E, typename F>
    auto raised(F f) {
      static_assert(
          !has_all_,
          "Handler for all errors should be installed last");

      // Push back new handler 'F' for error type 'E' to
      // already stored 'CatchHandlers'.
      return std::apply(
          [&](auto&&... catch_handler) {
            return Builder<
                has_all_,
                CatchHandlers_...,
                Handler<E, F>>{
                std::make_tuple(
                    std::move(catch_handler)...,
                    Handler<E, F>{std::move(f)})};
          },
          std::move(catch_handlers_));
    }

    template <typename F>
    auto all(F f) {
      static_assert(!has_all_, "Duplicate 'all'");

      // Push back new handler 'F' for all error types (using 'void')
      // to already stored 'CatchHandlers'.
      return std::apply(
          [&](auto&&... callback) {
            return Builder<
                true,
                CatchHandlers_...,
                Handler<void, F>>{
                std::make_tuple(
                    std::move(callback)...,
                    Handler<void, F>{
                        std::move(f)})};
          },
          std::move(catch_handlers_));
    }

    std::tuple<CatchHandlers_...> catch_handlers_;
  };
};

////////////////////////////////////////////////////////////////////////

inline auto Catch() {
  return _Catch::Builder<>{};
}

template <typename F>
auto Catch(F f) {
  return Catch().all(std::move(f));
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
