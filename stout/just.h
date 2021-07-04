#pragma once

#include "stout/eventual.h"

namespace stout {
namespace eventuals {

template <typename T>
auto Just(T t)
{
  return Eventual<T>()
    .context(std::move(t))
    .start([](auto& t, auto& k, auto&&...) {
      eventuals::succeed(k, std::move(t));
    });
}

} // namespace eventuals {
} // namespace stout {