#include <type_traits>

#include "near_laugh/application.hpp"

static_assert(!std::is_copy_constructible_v<near_laugh::Application>);
static_assert(std::is_default_constructible_v<near_laugh::RuntimeConfig>);

int main() { return 0; }
