#pragma once

#include <any>
#include <memory> // For 'std::unique_ptr'.
#include <optional>
#include <tuple>
#include <variant> // For 'std::monostate'.

#include "eventuals/eventual.h"
#include "eventuals/then.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

struct _Catch {
  template <typename Error_, typename Handler_>
  struct Handler {
    using error_type = Error_;
    Handler_ handler_;
  };

  template <bool Eventual_, typename HandlerResult_, typename K_>
  struct ContinuationIf {
    using continuation = std::monostate;
  };

  template <typename HandlerResult_, typename K_>
  struct ContinuationIf<true, HandlerResult_, K_> {
    using continuation =
        decltype(std::declval<HandlerResult_>().template k<void>(
            std::declval<_Then::Adaptor<K_>>()));
  };

  template <typename K_, typename Handler_, typename Error_>
  struct HandlerWithK {
    using error_type = Error_;

    HandlerWithK(Handler_&& handler)
      : handler_(std::move(handler)) {}

    template <typename K, typename... Args>
    void BuildAndInvoke(K& k, Interrupt* interrupt, Args&&... args) {
      using HandlerInvokeResult =
          typename std::invoke_result<Handler_, Args...>::type;

      if constexpr (HasValueFrom<HandlerInvokeResult>::value) {
        constructed_k_.emplace(
            (handler_(std::forward<Args>(args)...)
                 .template k<void>(_Then::Adaptor<K>{k})));

        if (interrupt != nullptr) {
          constructed_k_->Register(*interrupt);
        }

        constructed_k_->Start();
      } else if constexpr (std::is_void_v<HandlerInvokeResult>) {
        handler_(std::forward<Args>(args)...);
        k.Fail();
      } else {
        k.Start(handler_(std::forward<Args>(args)...));
      }
    }

    template <typename K, typename E>
    bool Handle(K& k, Interrupt* interrupt, E&& error) {
      if constexpr (
          std::is_same_v<Error_, E> || std::is_base_of_v<Error_, E>) {
        BuildAndInvoke(k, interrupt, std::move(error));
        return true;
      } else if constexpr (std::is_same_v<E, std::exception_ptr>) {
        try {
          std::rethrow_exception(error);
        } catch (Error_ e) {
          BuildAndInvoke(k, interrupt, std::move(e));
          return true;
        } catch (...) {
          // if constexpr (std::is_same_v<Error_, AllHandler>) {
          //   BuildAndInvoke(k, interrupt, std::current_exception());
          //   return true;
          // } else {
          //   return false;
          // }
          return false;
        }
      } else {
        return false;
      }
    }

    // Only 'all' hander can be executed with no errors.
    template <typename K>
    bool Handle(K& k, Interrupt* interrupt) {
      BuildAndInvoke(k, interrupt);
      return true;
    }

    Handler_ handler_;

    using E_ = typename std::conditional_t<
        std::is_void_v<Error_>,
        std::invoke_result<Handler_>,
        std::invoke_result<Handler_, Error_>>::type;

    using ConstructedK_ =
        typename ContinuationIf<HasValueFrom<E_>::value, E_, K_>::continuation;

    std::optional<ConstructedK_> constructed_k_;
  };

  ////////////////////////////////////////////////////////////////////////

  template <
      typename K_,
      typename AllHandler_,
      typename CatchHandlersWithKTuple_>
  struct Continuation {
    static constexpr bool HasAll =
        !std::is_same_v<AllHandler_, std::monostate>;

    template <
        typename Error_,
        typename CatchHandler_,
        typename... Tail>
    void UnpackInvokeHelper(
        Error_&& error,
        CatchHandler_&& handler,
        Tail&&... tail) {
      if (!processed_) {
        processed_ = handler.Handle(k_, interrupt_, std::move(error));
        if (!processed_) {
          UnpackInvokeHelper<Error_>(
              std::move(error),
              std::forward<Tail>(tail)...);
        }
      }
    }

    template <typename Error_, typename CatchHandler_>
    void UnpackInvokeHelper(
        Error_&& error,
        CatchHandler_&& handler) {
      if (!processed_) {
        processed_ = handler.Handle(k_, interrupt_, std::move(error));
      }
    }

    template <typename... Args>
    void Start(Args&&... args) {
      k_.Start(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Fail(Args&&... args) {
      static_assert(
          sizeof...(args) == 0 || sizeof...(args) == 1,
          "Catch only supports 0 or 1 argument, but found > 1");

      if constexpr (HasAll) {
        if constexpr (sizeof...(args) == 1) {
          all_handler_ = std::make_any<HandlerWithK<K_, AllHandler_, Args...>>(
              // 'AllHandler_' there is just a
              // lambda function for 'all' handler.
              std::move(std::any_cast<AllHandler_>(std::move(all_handler_))));
        } else {
          all_handler_ = std::make_any<HandlerWithK<K_, AllHandler_, void>>(
              std::any_cast<AllHandler_>(std::move(all_handler_)));
        }
      }

      if constexpr (sizeof...(args) == 1) {
        std::apply(
            [&](auto&&... catch_handlers) {
              UnpackInvokeHelper(
                  std::forward<decltype(args)>(args)...,
                  std::forward<decltype(catch_handlers)>(catch_handlers)...);
            },
            std::move(catch_handlers_));

        if constexpr (HasAll) {
          if (!processed_) {
            processed_ = std::any_cast<
                             HandlerWithK<
                                 K_,
                                 AllHandler_,
                                 Args...>>(std::move(all_handler_))
                             .Handle(
                                 k_,
                                 interrupt_,
                                 std::forward<decltype(args)>(args)...);
          }
        }
      } else {
        // Only 'all' hander can be executed with no errors.
        if constexpr (HasAll) {
          processed_ = std::any_cast<
                           HandlerWithK<
                               K_,
                               AllHandler_,
                               void>>(std::move(all_handler_))
                           .Handle(k_, interrupt_);
        } else {
          k_.Fail(std::forward<decltype(args)>(args)...);
        }
      }

      // Should be executed if no handlers executed before.
      if (!processed_) {
        k_.Fail(std::forward<decltype(args)>(args)...);
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
    CatchHandlersWithKTuple_ catch_handlers_;
    std::any all_handler_;
    bool processed_ = false;

    Interrupt* interrupt_ = nullptr;
  };

  template <
      // Can't use 'void' instead because of storing in 'std::optional'.
      typename AllHandler_ = std::monostate,
      typename... CatchHandlers_>
  struct Builder {
    template <typename Arg>
    using ValueFrom = Arg;

    static constexpr bool HasAll =
        !std::is_same_v<AllHandler_, std::monostate>;

    template <size_t i, typename K, typename Tuple>
    auto AddTypeToTuple(Tuple&& tuple) {
      if constexpr (i < std::tuple_size_v<decltype(catch_handlers_)>) {
        auto current = std::get<i>(catch_handlers_);
        auto add_curent = std::apply(
            [&](auto&&... exist) {
              return std::make_tuple(
                  std::move(exist)...,
                  HandlerWithK<
                      K,
                      decltype(current.handler_),
                      typename decltype(current)::error_type>{
                      std::move(current.handler_)});
            },
            tuple);

        return AddTypeToTuple<i + 1, K>(std::move(add_curent));
      } else
        return std::move(tuple);
    }

    template <size_t i, typename K>
    auto AddTypeToTuple() {
      static_assert(
          i < std::tuple_size_v<decltype(catch_handlers_)>,
          "No catch handlers specified");

      auto current = std::get<i>(catch_handlers_);

      return AddTypeToTuple<i + 1, K>(
          std::make_tuple(
              HandlerWithK<
                  K,
                  decltype(current.handler_),
                  typename decltype(current)::error_type>{
                  std::move(current.handler_)}));
    }

    template <typename Arg, typename K>
    auto k(K k) && {
      static_assert(
          sizeof...(CatchHandlers_) > 0 || HasAll,
          "No handlers were specified for 'Catch'");

      // We want to store 'K' at 'CatchHandler' because if the
      // handler returns an 'Eventual', we need to provide the next
      // 'k' for it.
      // 'AddTypeToTuple' just creates new tuple with 'CatchHandlers',
      // but also provides 'K' type for each.
      auto handlers_with_k = AddTypeToTuple<0, K>();

      return Continuation<K, AllHandler_, decltype(handlers_with_k)>{
          std::move(k),
          std::move(handlers_with_k),
          std::make_any<AllHandler_>(std::move(all_handler_))};
    }

    template <typename E, typename F>
    auto raised(F f) {
      static_assert(
          !HasAll,
          "Handler for all errors should be installed last");

      // Push back new handler 'F' for error type 'E' to
      // already stored 'CatchHandlers'.
      return std::apply(
          [&](auto&&... catch_handler) {
            return Builder<
                std::monostate,
                CatchHandlers_...,
                Handler<E, F>>{
                std::make_tuple(
                    std::move(catch_handler)...,
                    Handler<E, F>{std::move(f)}),
                std::move(all_handler_)};
          },
          std::move(catch_handlers_));
    }

    template <typename F>
    auto all(F f) {
      static_assert(!HasAll, "Duplicate 'all'");

      return Builder<
          F,
          CatchHandlers_...>{std::move(catch_handlers_), std::move(f)};
    }

    std::tuple<CatchHandlers_...> catch_handlers_;
    AllHandler_ all_handler_;
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
