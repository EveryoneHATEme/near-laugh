#ifndef CORE_TEXT_UTF8_HPP
#define CORE_TEXT_UTF8_HPP
#include <string_view>
#include <vector>
[[nodiscard]] std::vector<char32_t> captionScalars(std::string_view text);
#endif
