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

struct _Catch {
  template <typename K_, typename Error_, typename F_>
  struct Handler {
    using Error = Error_;

    Handler(F_ f)
      : f_(std::move(f)) {}

    // Helper to convert a catch handler to a new 'K'.
    template <typename K>
    auto Convert() && {
      return Handler<K, Error_, F_>{std::move(f_)};
    }

    template <typename E>
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
          adapted_.emplace(Then(std::move(f_)).template k<Error_>(std::move(k)));

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

    bool Handle(K_&& k, Interrupt* interrupt) {
      if constexpr (std::is_void_v<Error_>) {
        adapted_.emplace(Then(std::move(f_)).template k<Error_>(std::move(k)));

        if (interrupt != nullptr) {
          adapted_->Register(*interrupt);
        }

        adapted_->Start();

        return true;
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
  struct Continuation {
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
          using AllE = decltype(all_f_(std::forward<Args>(args)...)
                                    .template k<void>(_Then::Adaptor<K_>{k_}));

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

  template <typename AllF_, typename... CatchHandlers_>
  struct Builder {
    template <typename Arg>
    using ValueFrom = Arg;

    template <typename AllF, typename... CatchHandlers>
    static auto create(
        std::tuple<CatchHandlers...>&& catch_handlers,
        AllF all_f) {
      return Builder<AllF, CatchHandlers...>{
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

      return create(
          std::tuple_cat(
              std::move(catch_handlers_),
              std::tuple{Handler<Undefined, Error, F>{std::move(f)}}),
          std::move(all_f_));
    }

    template <typename F>
    auto all(F f) {
      static_assert(IsUndefined<AllF_>::value, "Duplicate 'all'");

      return create(std::move(catch_handlers_), std::move(f));
    }

    std::tuple<CatchHandlers_...> catch_handlers_;
    AllF_ all_f_;
  };
};

////////////////////////////////////////////////////////////////////////

inline auto Catch() {
  return _Catch::Builder<Undefined>{};
}

template <typename F>
auto Catch(F f) {
  return Catch().all(std::move(f));
}

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
