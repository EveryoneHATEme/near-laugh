#ifndef CORE_TEXT_UTF8_HPP
#define CORE_TEXT_UTF8_HPP
#include <string_view>
#include <vector>
[[nodiscard]] std::vector<char32_t> captionScalars(std::string_view text);
// Household fields have an absolute 480-scalar bound. Only pages allow LF.
[[nodiscard]] std::vector<char32_t> readableScalars(
    std::string_view text, bool allow_newlines = false);
#endif
