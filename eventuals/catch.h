#pragma once

#include <memory> // For 'std::unique_ptr'.
#include <optional>
#include <tuple>
#include <variant> // For 'std::monostate'.

#include "eventuals/callback.h"
#include "eventuals/eventual.h"
#include "eventuals/then.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

struct _Catch final {
  template <typename K_, typename Error_, typename F_>
  struct Handler final {
    using Error = Error_;
    using Function = F_;

    Handler(F_ f)
      : f_(std::move(f)) {}

    // Helper to convert a catch handler to a new 'K'.
    template <typename K>
    auto Convert() && {
      return Handler<K, Error_, F_>{std::move(f_)};
    }

    template <typename E>
    // NOTE: we're _must_ explicitly take 'k' and 'e' by rvalue reference here
    // because we do _not_ want the compiler to move the 'k' and 'e' when
    // calling this function in the event that we _don't_ end up handling the
    // error.
    bool Handle(K_&& k, Interrupt* interrupt, E&& e) {
      if constexpr (
          std::is_same_v<Error_, E> || std::is_base_of_v<Error_, E>) {
        adapted_.emplace(Then(std::move(f_)).template k<Error_>(std::move(k)));

        if (interrupt != nullptr) {
          adapted_->Register(*interrupt);
        }

        adapted_->Start(std::forward<E>(e));

        return true;
      } else if constexpr (std::is_same_v<E, std::exception_ptr>) {
        try {
          std::rethrow_exception(e);
        }
        // Catch by reference because if 'Error_' is a polymorphic
        // type it can't be caught by value.
        catch (Error_& error) {
          adapted_.emplace(
              Then(std::move(f_))
                  .template k<Error_>(std::move(k)));

          if (interrupt != nullptr) {
            adapted_->Register(*interrupt);
          }

          adapted_->Start(std::move(error));

          return true;
        } catch (...) {
          return false;
        }
      } else {
        // Just to avoid '-Werror=unused-but-set-parameter' warning.
        (void) interrupt;
        return false;
      }
    }

    F_ f_;

    using Adapted_ =
        decltype(Then(std::move(f_)).template k<Error_>(std::declval<K_>()));

    std::optional<Adapted_> adapted_;
  };

  ////////////////////////////////////////////////////////////////////////

  template <
      typename K_,
      typename AllF_,
      typename... CatchHandlers_>
  struct Continuation final {
    Continuation(
        K_ k,
        std::tuple<CatchHandlers_...>&& catch_handlers,
        AllF_ all_f)
      : catch_handlers_(std::move(catch_handlers)),
        all_f_(std::move(all_f)),
        k_(std::move(k)) {}

    template <typename... Args>
    void Start(Args&&... args) {
      k_.Start(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Fail(Args&&... args) {
      static_assert(
          sizeof...(args) == 0 || sizeof...(args) == 1,
          "Catch only supports 0 or 1 argument, but found > 1");

      std::apply(
          [&](auto&... catch_handler) {
            // Using a fold expression to simplify the iteration.
            ([&](auto& catch_handler) {
              if (!handled_) {
                handled_ = catch_handler.Handle(
                    std::move(k_),
                    interrupt_,
                    std::forward<decltype(args)>(args)...);
              }
            }(catch_handler),
             ...);
          },
          catch_handlers_);

      // Try the "all" handler if present.
      if (!handled_) {
        if constexpr (!IsUndefined<AllF_>::value) {
          using AllInvokeResult = std::invoke_result_t<AllF_, Args...>;

          if constexpr (std::is_void_v<AllInvokeResult>) {
            all_f_(std::forward<Args>(args)...);
            k_.Fail(
                std::make_exception_ptr(
                    "'all' handler empty propagate error"));
          } else {
            if constexpr (HasValueFrom<AllInvokeResult>::value) {
              using AllE = decltype(all_f_(std::forward<Args>(args)...)
                                        .template k<void>(
                                            _Then::Adaptor<K_>{k_}));

              // TODO(benh): propagate eventual errors so we don't need to
              // allocate on the heap in order to type erase.
              all_e_ = std::unique_ptr<void, Callback<void*>>(
                  new AllE(all_f_(std::forward<Args>(args)...)
                               .template k<void>(_Then::Adaptor<K_>{k_})),
                  [](void* e) {
                    delete static_cast<AllE*>(e);
                  });

              auto* e = static_cast<AllE*>(all_e_.get());

              if (interrupt_ != nullptr) {
                e->Register(*interrupt_);
              }

              e->Start();
            } else {
              k_.Start(all_f_(std::forward<Args>(args)...));
            }
          }
          handled_ = true;
        }
      }

      if (!handled_) {
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

    std::tuple<CatchHandlers_...> catch_handlers_;
    AllF_ all_f_;

    bool handled_ = false;

    Interrupt* interrupt_ = nullptr;

    // TODO(benh): propagate eventual errors so we don't need to
    // allocate on the heap in order to type erase.
    std::unique_ptr<void, Callback<void*>> all_e_;

    // NOTE: we store 'k_' as the _last_ member so it will be
    // destructed _first_ and thus we won't have any use-after-delete
    // issues during destruction of 'k_' if it holds any references or
    // pointers to any (or within any) of the above members.
    K_ k_;
  };

  template <typename...>
  struct UnifyUnpacker {
    using type = void;
  };

  template <typename LeftHandler, typename RightHandler, typename... Handlers>
  struct UnifyUnpacker<LeftHandler, RightHandler, Handlers...> {
    template <typename Left, typename Right>
    using Unify_ = typename std::conditional_t<
        std::is_same_v<Left, Right>,
        type_identity<Left>,
        std::conditional_t<
            std::is_void_v<Left>,
            type_identity<Right>,
            std::enable_if<std::is_void_v<Right>, Left>>>::type;

    using Left_ = std::invoke_result_t<
        typename LeftHandler::Function,
        typename LeftHandler::Error>;
    using Right_ = std::invoke_result_t<
        typename RightHandler::Function,
        typename RightHandler::Error>;

    using current_ = Unify_<
        ValueFromMaybeComposable<Left_, void>,
        ValueFromMaybeComposable<Right_, void>>;

    using type = Unify_<current_, typename UnifyUnpacker<Handlers...>::type>;
  };

  template <typename ReturnType_, typename AllF_, typename... CatchHandlers_>
  struct Builder final {
    template <typename Arg>
    using ValueFrom = ReturnType_;

    template <typename ReturnType, typename AllF, typename... CatchHandlers>
    static auto create(
        std::tuple<CatchHandlers...>&& catch_handlers,
        AllF all_f) {
      return Builder<ReturnType, AllF, CatchHandlers...>{
          std::move(catch_handlers),
          std::move(all_f)};
    }

    template <typename Arg, typename K>
    auto k(K k) && {
      static_assert(
          sizeof...(CatchHandlers_) > 0 || !IsUndefined<AllF_>::value,
          "No handlers were specified for 'Catch'");

      // Convert each catch handler to one with 'K' instead of
      // 'Undefined' and then return 'Continuation'.
      return std::apply(
          [&](auto&&... catch_handler) {
            return Continuation<
                K,
                AllF_,
                decltype(std::move(catch_handler).template Convert<K>())...>(
                std::move(k),
                std::tuple{std::move(catch_handler).template Convert<K>()...},
                std::move(all_f_));
          },
          std::move(catch_handlers_));
    }

    template <typename Error, typename F>
    auto raised(F f) {
      static_assert(
          IsUndefined<AllF_>::value,
          "'all' handler must be installed last");

      static_assert(
          std::is_invocable_v<F, Error>,
          "Catch handler can not be invoked with your specified error");

      using Unified = std::conditional_t<
          sizeof...(CatchHandlers_) >= 2,
          typename UnifyUnpacker<
              Handler<Undefined, Error, F>,
              CatchHandlers_...>::type,
          ReturnType_>;

      using InitialType = ValueFromMaybeComposable<
          std::invoke_result_t<F, Error>,
          void>;

      return create<
          std::conditional_t<
              IsUndefined<ReturnType_>::value,
              InitialType,
              Unified>>(
          std::tuple_cat(
              std::move(catch_handlers_),
              std::tuple{Handler<Undefined, Error, F>{std::move(f)}}),
          std::move(all_f_));
    }

    template <typename F>
    auto all(F f) {
      static_assert(IsUndefined<AllF_>::value, "Duplicate 'all'");

      // We can use any parameter type for 'all' handler,
      // since it should be invocable for any errors.
      struct ArbitraryTypeForTestingInvocable {};
      static_assert(
          std::is_invocable_v<F, ArbitraryTypeForTestingInvocable>,
          "Function specified at 'all' must support invoking with any type");

      using Unified = std::conditional_t<
          sizeof...(CatchHandlers_) >= 2,
          typename UnifyUnpacker<
              Handler<Undefined, ArbitraryTypeForTestingInvocable, F>,
              CatchHandlers_...>::type,
          ReturnType_>;

      using InitialType = ValueFromMaybeComposable<
          std::invoke_result_t<F, ArbitraryTypeForTestingInvocable>,
          void>;

      return create<
          std::conditional_t<
              IsUndefined<ReturnType_>::value,
              InitialType,
              Unified>>(std::move(catch_handlers_), std::move(f));
    }

    std::tuple<CatchHandlers_...> catch_handlers_;
    AllF_ all_f_;
  };
};

////////////////////////////////////////////////////////////////////////

inline auto Catch() {
  return _Catch::Builder<Undefined, Undefined>{};
}

template <typename F>
auto Catch(F f) {
  return Catch().all(std::move(f));
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
