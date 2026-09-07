#include "core/text/utf8.hpp"

#include <stdexcept>

std::vector<char32_t> captionScalars(std::string_view text) {
  std::vector<char32_t> scalars;
  if (text.size() > 640)
    throw std::invalid_argument("caption UTF-8 exceeds bounded text size");
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i++]);
    unsigned trailing{};
    char32_t scalar{};
    char32_t minimum{};
    if (lead < 0x80)
      scalar = lead;
    else if (lead >= 0xc2 && lead <= 0xdf) {
      trailing = 1;
      scalar = lead & 31;
      minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      trailing = 2;
      scalar = lead & 15;
      minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      trailing = 3;
      scalar = lead & 7;
      minimum = 0x10000;
    } else
      throw std::invalid_argument("invalid UTF-8 leading byte");
    for (unsigned j = 0; j < trailing; ++j) {
      if (i == text.size())
        throw std::invalid_argument("truncated UTF-8 scalar");
      const auto byte = static_cast<unsigned char>(text[i++]);
      if ((byte & 0xc0) != 0x80)
        throw std::invalid_argument("invalid UTF-8 continuation byte");
      scalar = (scalar << 6) | (byte & 63);
    }
    if (scalar < minimum || scalar > 0x10ffff ||
        (scalar >= 0xd800 && scalar <= 0xdfff) || scalar < 0x20 ||
        (scalar >= 0x7f && scalar < 0xa0))
      throw std::invalid_argument(
          "invalid UTF-8 scalar or unsupported text control");
    scalars.push_back(scalar);
  }
  return scalars;
}
