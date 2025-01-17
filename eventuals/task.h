#pragma once

#include <functional> // For 'std::reference_wrapper'.
#include <memory> // For 'std::unique_ptr'.
#include <optional>
#include <tuple>
#include <variant>

#include "eventuals/eventual.h"
#include "eventuals/just.h"
#include "eventuals/raise.h"
#include "eventuals/terminal.h"
#include "eventuals/type-traits.h"

////////////////////////////////////////////////////////////////////////

namespace eventuals {

////////////////////////////////////////////////////////////////////////

template <typename E_, typename From_, typename To_>
struct HeapTask final {
  struct Adaptor final {
    Adaptor(
        Callback<function_type_t<void, To_>>* start,
        Callback<void(std::exception_ptr)>* fail,
        Callback<void()>* stop)
      : start_(start),
        fail_(fail),
        stop_(stop) {}

    template <typename... Args>
    void Start(Args&&... args) {
      (*start_)(std::forward<Args>(args)...);
    }

    template <typename Error>
    void Fail(Error&& error) {
      (*fail_)(
          make_exception_ptr_or_forward(
              std::forward<Error>(error)));
    }

    // NOTE: overload so we don't create nested std::exception_ptr.
    void Fail(std::exception_ptr exception) {
      (*fail_)(std::move(exception));
    }

    void Stop() {
      (*stop_)();
    }

    void Register(Interrupt&) {}

    Callback<function_type_t<void, To_>>* start_;
    Callback<void(std::exception_ptr)>* fail_;
    Callback<void()>* stop_;
  };

  HeapTask(E_ e)
    : adapted_(
        std::move(e).template k<From_>(
            Adaptor{&start_, &fail_, &stop_})) {}

  void Start(
      std::conditional_t<
          std::is_void_v<From_>,
          std::monostate,
          From_>&& arg,
      Interrupt& interrupt,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    start_ = std::move(start);
    fail_ = std::move(fail);
    stop_ = std::move(stop);

    // TODO(benh): clarify the semantics of whether or not calling
    // 'Register()' more than once is well-defined.
    adapted_.Register(interrupt);

    if constexpr (std::is_void_v<From_>) {
      adapted_.Start();
    } else {
      adapted_.Start(std::move(arg));
    }
  }

  void Fail(
      Interrupt& interrupt,
      std::exception_ptr&& exception,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    start_ = std::move(start);
    fail_ = std::move(fail);
    stop_ = std::move(stop);

    // TODO(benh): clarify the semantics of whether or not calling
    // 'Register()' more than once is well-defined.
    adapted_.Register(interrupt);

    adapted_.Fail(std::move(exception));
  }

  void Stop(
      Interrupt& interrupt,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    start_ = std::move(start);
    fail_ = std::move(fail);
    stop_ = std::move(stop);

    // TODO(benh): clarify the semantics of whether or not calling
    // 'Register()' more than once is well-defined.
    adapted_.Register(interrupt);

    adapted_.Stop();
  }

  Callback<function_type_t<void, To_>> start_;
  Callback<void(std::exception_ptr)> fail_;
  Callback<void()> stop_;

  using Adapted_ = decltype(std::declval<E_>().template k<From_>(
      std::declval<Adaptor>()));

  Adapted_ adapted_;
};

////////////////////////////////////////////////////////////////////////

// Type used to identify when using a 'Task::Failure()' in order to
// properly type-check with 'static_assert()'.
struct _TaskFailure final {
  _TaskFailure() = delete;
};

////////////////////////////////////////////////////////////////////////

template <typename T>
using MonostateIfVoidOr =
    std::conditional_t<
        std::is_void_v<T>,
        std::monostate,
        T>;

template <typename T>
using MonostateIfVoidOrReferenceWrapperOr =
    std::conditional_t<
        std::is_void_v<T>,
        std::monostate,
        std::conditional_t<
            !std::is_reference_v<T>,
            T,
            std::reference_wrapper<
                std::remove_reference_t<T>>>>;

////////////////////////////////////////////////////////////////////////

struct _TaskFromToWith final {
  // Since we move lambda function at 'Composable' constructor we need to
  // specify the callback that should be triggered on the produced eventual.
  // For this reason we use 'Action'.
  enum class Action {
    Start = 0,
    Stop = 1,
    Fail = 2,
  };

  // Using templated type name to allow using both
  // in 'Composable' and 'Continuation'.
  template <typename From, typename To, typename... Args>
  using DispatchCallback =
      Callback<void(
          Action,
          std::optional<std::exception_ptr>&&,
          Args&&...,
          // Can't have a 'void' argument type
          // so we are using 'std::monostate'.
          std::optional<
              std::conditional_t<
                  std::is_void_v<From>,
                  std::monostate,
                  From>>&&,
          std::unique_ptr<void, Callback<void(void*)>>&,
          Interrupt&,
          Callback<function_type_t<void, To>>&&,
          Callback<void(std::exception_ptr)>&&,
          Callback<void()>&&)>;

  template <
      typename K_,
      typename From_,
      typename To_,
      typename Errors_,
      typename... Args_>
  struct Continuation final {
    Continuation(
        K_ k,
        std::tuple<Args_...> args,
        std::variant<
            MonostateIfVoidOrReferenceWrapperOr<To_>,
            DispatchCallback<From_, To_, Args_...>>
            value_or_dispatch)
      : args_(std::move(args)),
        value_or_dispatch_(std::move(value_or_dispatch)),
        k_(std::move(k)) {}

    template <typename... From>
    void Start(From&&... from) {
      switch (value_or_dispatch_.index()) {
        case 0:
          if constexpr (!std::is_void_v<To_>) {
            // Need cast to 'To_' to cast to reference type, otherwise
            // it casts to non-reference.
            k_.Start(
                static_cast<To_>(
                    std::move(std::get<0>(value_or_dispatch_))));
          } else {
            k_.Start();
          }
          break;
        case 1:
          if constexpr (std::is_void_v<From_>) {
            Dispatch(Action::Start, std::monostate{});
          } else {
            static_assert(
                sizeof...(from) > 0,
                "Expecting \"from\" argument for 'Task<From, To>' "
                "but no argument passed");
            Dispatch(Action::Start, std::forward<From>(from)...);
          }
          break;
        default:
          LOG(FATAL) << "unreachable";
      }
    }

    template <typename Error>
    void Fail(Error&& error) {
      std::exception_ptr exception;

      exception = make_exception_ptr_or_forward(
          std::forward<Error>(error));

      Dispatch(Action::Fail, std::nullopt, std::move(exception));
    }

    void Stop() {
      Dispatch(Action::Stop);
    }

    void Register(Interrupt& interrupt) {
      interrupt_ = &interrupt;
      k_.Register(interrupt);
    }

    void Dispatch(
        Action action,
        std::optional<MonostateIfVoidOr<From_>>&& from = std::nullopt,
        std::optional<std::exception_ptr>&& exception = std::nullopt) {
      CHECK_EQ(value_or_dispatch_.index(), 1u);

      std::apply(
          [&](auto&&... args) {
            std::get<1>(value_or_dispatch_)(
                action,
                std::move(exception),
                std::forward<decltype(args)>(args)...,
                std::forward<decltype(from)>(from),
                e_,
                *interrupt_,
                [this](auto&&... args) {
                  k_.Start(std::forward<decltype(args)>(args)...);
                },
                [this](std::exception_ptr error) {
                  k_.Fail(std::move(error));
                },
                [this]() {
                  k_.Stop();
                });
          },
          std::move(args_));
    }

    std::tuple<Args_...> args_;

    // The 'dispatch_' is a `std::variant` because its either a function that
    // creates an eventual or the value from 'Task::Success' that should be
    // passed on to the continuation.
    std::variant<
        MonostateIfVoidOrReferenceWrapperOr<To_>,
        DispatchCallback<From_, To_, Args_...>>
        value_or_dispatch_;

    std::unique_ptr<void, Callback<void(void*)>> e_;
    Interrupt* interrupt_ = nullptr;

    // NOTE: we store 'k_' as the _last_ member so it will be
    // destructed _first_ and thus we won't have any use-after-delete
    // issues during destruction of 'k_' if it holds any references or
    // pointers to any (or within any) of the above members.
    K_ k_;
  };

  template <
      typename From_,
      typename To_,
      typename Errors_,
      typename... Args_>
  struct Composable final {
    template <typename>
    using ValueFrom = To_;

    template <typename Arg, typename Errors>
    using ErrorsFrom = tuple_types_union_t<Errors, Errors_>;

    Composable(MonostateIfVoidOrReferenceWrapperOr<To_> value)
      : value_or_dispatch_(std::move(value)) {}

    template <typename F>
    Composable(Args_... args, F f)
      : args_(std::tuple<Args_...>(std::move(args)...)) {
      static_assert(
          std::tuple_size<decltype(args_)>{} > 0 || std::is_invocable_v<F>,
          "'Task' expects a callable that takes no arguments");

      static_assert(
          std::tuple_size<decltype(args_)>{}
              || std::is_invocable_v<F, Args_...>,
          "'Task' expects a callable that "
          "takes the arguments specified");

      static_assert(
          sizeof(f) <= sizeof(void*),
          "'Task' expects a callable that "
          "can be captured in a 'Callback'");

      using E = decltype(std::apply(f, args_));

      using Value = typename E::template ValueFrom<From_>;

      using ErrorsFromE = typename E::template ErrorsFrom<From_, std::tuple<>>;

      static_assert(
          tuple_types_subset_subtype_v<ErrorsFromE, Errors_>,
          "Specified errors can't be raised within 'Task'");

      static_assert(
          std::disjunction_v<
              std::is_same<Value, _TaskFailure>,
              std::is_convertible<Value, To_>>,
          "eventual result type can not be converted into type of 'Task'");

      value_or_dispatch_ = [f = std::move(f)](
                               Action action,
                               std::optional<std::exception_ptr>&& exception,
                               Args_&&... args,
                               std::optional<MonostateIfVoidOr<From_>>&& arg,
                               std::unique_ptr<void, Callback<void(void*)>>& e_,
                               Interrupt& interrupt,
                               Callback<function_type_t<void, To_>>&& start,
                               Callback<void(std::exception_ptr)>&& fail,
                               Callback<void()>&& stop) mutable {
        if (!e_) {
          e_ = std::unique_ptr<void, Callback<void(void*)>>(
              new HeapTask<E, From_, To_>(f(std::move(args)...)),
              [](void* e) {
                delete static_cast<HeapTask<E, From_, To_>*>(e);
              });
        }

        auto* e = static_cast<HeapTask<E, From_, To_>*>(e_.get());

        switch (action) {
          case Action::Start:
            e->Start(
                std::move(arg.value()),
                interrupt,
                std::move(start),
                std::move(fail),
                std::move(stop));
            break;
          case Action::Fail:
            e->Fail(
                interrupt,
                std::move(exception.value()),
                std::move(start),
                std::move(fail),
                std::move(stop));
            break;
          case Action::Stop:
            e->Stop(
                interrupt,
                std::move(start),
                std::move(fail),
                std::move(stop));
            break;
          default:
            LOG(FATAL) << "unreachable";
        }
      };
    }

    Composable(
        std::optional<
            std::variant<
                MonostateIfVoidOrReferenceWrapperOr<To_>,
                DispatchCallback<From_, To_, Args_...>>>&& value_or_dispatch,
        std::tuple<Args_...>&& args)
      : value_or_dispatch_(std::move(value_or_dispatch)),
        args_(std::move(args)) {}

    template <typename Arg, typename K>
    auto k(K k) && {
      static_assert(
          !std::disjunction_v<IsUndefined<From_>, IsUndefined<To_>>,
          "'Task' 'From' or 'To' type is not specified");

      return Continuation<K, From_, To_, Errors_, Args_...>(
          std::move(k),
          std::move(args_),
          std::move(value_or_dispatch_.value()));
    }

    // See comment in `Continuation` for explanation of `dispatch_` member.
    // Using 'std::optional' because of implicitly deleted 'std::variant'
    // constructor.
    std::optional<
        std::variant<
            MonostateIfVoidOrReferenceWrapperOr<To_>,
            DispatchCallback<From_, To_, Args_...>>>
        value_or_dispatch_;

    std::tuple<Args_...> args_;
  };
};

////////////////////////////////////////////////////////////////////////

// A task can act BOTH as a composable or a continuation that can be
// started via '_Task::Start()'. If used as a continuation
// then it can't be moved after starting, just like all other
// continuations.
template <
    typename From_,
    typename To_,
    typename Errors_,
    typename... Args_>
class _Task final {
 public:
  template <typename Arg>
  using ValueFrom = To_;

  template <typename Arg, typename Errors>
  using ErrorsFrom = tuple_types_union_t<Errors, Errors_>;

  template <typename T>
  using From = std::enable_if_t<
      IsUndefined<From_>::value,
      _Task<T, To_, Errors_, Args_...>>;

  template <typename T>
  using To = std::enable_if_t<
      IsUndefined<To_>::value,
      _Task<From_, T, Errors_, Args_...>>;

  template <typename... Errors>
  using Raises = std::enable_if_t<
      std::tuple_size_v<Errors_> == 0,
      _Task<From_, To_, std::tuple<Errors...>, Args_...>>;

  template <typename... Args>
  using With = std::enable_if_t<
      sizeof...(Args_) == 0,
      _Task<From_, To_, Errors_, Args...>>;

  template <typename T>
  using Of = std::enable_if_t<
      std::conjunction_v<IsUndefined<From_>, IsUndefined<To_>>,
      _Task<void, T, Errors_, Args_...>>;

  template <typename F>
  _Task(Args_... args, F f)
    : e_(std::move(args)..., std::move(f)) {}

  _Task(MonostateIfVoidOrReferenceWrapperOr<To_> value)
    : e_(std::move(value)) {}

  template <
      typename E,
      std::enable_if_t<tuple_types_subset_subtype_v<E, Errors_>, int> = 0>
  _Task(_Task<From_, To_, E, Args_...>&& that)
    : e_(std::move(that.e_.value_or_dispatch_), std::move(that.e_.args_)) {
    CHECK(!k_.has_value()) << "moving after starting";
  }

  _Task(_Task&& that)
    : e_(std::move(that.e_)) {
    CHECK(!k_.has_value()) << "moving after starting";
  }

  ~_Task() = default;

  template <typename Arg, typename K>
  auto k(K k) && {
    static_assert(
        !std::disjunction_v<IsUndefined<From_>, IsUndefined<To_>>,
        "'Task' 'From' or 'To' type is not specified");

    return std::move(e_).template k<Arg>(std::move(k));
  }

  void Start(
      Interrupt& interrupt,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    k_.emplace(Build(
        std::move(e_)
        | Terminal()
              .start(std::move(start))
              .fail(std::move(fail))
              .stop(std::move(stop))));

    k_->Register(interrupt);

    k_->Start();
  }

  template <typename Error>
  void Fail(
      Error&& error,
      Interrupt& interrupt,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    static_assert(
        std::disjunction_v<
            std::is_same<std::exception_ptr, std::decay_t<Error>>,
            std::is_base_of<std::exception, std::decay_t<Error>>>,
        "Expecting a type derived from std::exception ");

    static_assert(
        std::disjunction_v<
            std::is_same<std::exception_ptr, std::decay_t<Error>>,
            tuple_types_contains_subtype<std::decay_t<Error>, Errors_>>,
        "Error is not specified in 'Raises'");

    k_.emplace(Build(
        std::move(e_)
        | Terminal()
              .start(std::move(start))
              .fail(std::move(fail))
              .stop(std::move(stop))));

    k_->Register(interrupt);

    k_->Fail(std::forward<Error>(error));
  }

  void Stop(
      Interrupt& interrupt,
      Callback<function_type_t<void, To_>>&& start,
      Callback<void(std::exception_ptr)>&& fail,
      Callback<void()>&& stop) {
    k_.emplace(Build(
        std::move(e_)
        | Terminal()
              .start(std::move(start))
              .fail(std::move(fail))
              .stop(std::move(stop))));

    k_->Register(interrupt);

    k_->Stop();
  }

  // NOTE: should only be used in tests!
  auto operator*() && {
    auto [future, k] = Terminate(std::move(e_));

    k.Start();

    return future.get();
  }

  // Helpers for synchronous tasks.

  template <typename Value>
  static auto Success(Value value) {
    return _Task<
        void,
        Value,
        std::tuple<>>(std::move(value));
  }

  template <typename Value>
  static auto Success(std::reference_wrapper<Value> value) {
    return _Task<
        void,
        Value&,
        std::tuple<>>(std::move(value));
  }

  static auto Success() {
    return _Task<
        void,
        void,
        std::tuple<>>(std::monostate{});
  }

  template <typename Error>
  static auto Failure(Error error) {
    static_assert(
        std::is_base_of_v<std::exception, std::decay_t<Error>>,
        "Expecting a type derived from std::exception");

    // TODO(benh): optimize away heap allocation.
    // If we store an error using 'std::exception_ptr' it is also a memory
    // allocation, otherwise we need to store one more template parameter
    // for the 'Error' type.
    return [error = std::make_unique<Error>(std::move(error))]() mutable {
      return Eventual<_TaskFailure>()
          .raises<Error>()
          .start([&](auto& k) mutable {
            k.Fail(Error(std::move(*error)));
          });
    };
  }

  static auto Failure(const std::string& s) {
    return Failure(std::runtime_error(s));
  }

  static auto Failure(char* s) {
    return Failure(std::runtime_error(s));
  }

  static auto Failure(const char* s) {
    return Failure(std::runtime_error(s));
  }

 private:
  // To make possible constructing from another '_Task' with
  // the different 'std::tuple' of error types.
  template <typename, typename, typename, typename...>
  friend class _Task;

  std::conditional_t<
      std::disjunction_v<IsUndefined<From_>, IsUndefined<To_>>,
      decltype(Eventual<Undefined>()),
      _TaskFromToWith::Composable<From_, To_, Errors_, Args_...>>
      e_;

  // NOTE: if 'Task::Start()' is invoked then 'Task' becomes not just
  // a composable but also a continuation which has a terminal made up
  // of the callbacks passed to 'Task::Start()'.
  using K_ = std::conditional_t<
      std::disjunction_v<IsUndefined<From_>, IsUndefined<To_>>,
      Undefined,
      decltype(Build(
          std::move(e_)
          | Terminal()
                .start(std::declval<Callback<function_type_t<void, To_>>&&>())
                .fail(std::declval<Callback<void(std::exception_ptr)>&&>())
                .stop(std::declval<Callback<void()>&&>())))>;

  // NOTE: we store 'k_' as the _last_ member so it will be
  // destructed _first_ and thus we won't have any use-after-delete
  // issues during destruction of 'k_' if it holds any references or
  // pointers to any (or within any) of the above members.
  std::optional<K_> k_;
};

using Task = _Task<Undefined, Undefined, std::tuple<>>;

////////////////////////////////////////////////////////////////////////

} // namespace eventuals

////////////////////////////////////////////////////////////////////////
