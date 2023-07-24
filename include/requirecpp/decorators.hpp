#pragma once

#include <memory>

namespace requirecpp::decorator {

template <typename T, auto... State>
struct Tagged {
  std::shared_ptr<T> object;
};

}  // namespace requirecpp::decorator
