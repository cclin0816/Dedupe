#include "parse_size.hpp"

#include <array>
#include <stdexcept>

namespace utils {

// fast pow of unsigned long
std::size_t pow_ul(std::size_t base, std::size_t exp) {
  std::size_t result = 1;
  while (exp != 0) {
    if (exp & 1) {
      result *= base;
    }
    exp = exp >> 1;
    base *= base;
  }
  return result;
}

bool is_num(char c) { return c >= '0' && c <= '9'; }

std::size_t parse_size(const std::string size_str) {
  std::size_t size_num = 0;
  const auto size_len = size_str.size();
  const std::array<char, 8> unit_dict({'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'});
  bool as_bit = false;
  bool as_bibyte = false;
  std::size_t scale = 0;

  for (std::size_t i = 0; i < size_len; i++) {
    auto c = size_str[i];
    if (is_num(c)) {
      size_num = size_num * 10 + (c - '0');
    } else {
      if (i == 0) {
        throw std::invalid_argument("Invalid size string: " + size_str);
      }
      for (std::size_t j = 0; j < unit_dict.size(); j++) {
        if (c == unit_dict[j] || c == unit_dict[j] + 32) {
          scale = j + 1;
          i++;
          break;
        }
      }
      if (i < size_len) {
        c = size_str[i];
        if (c == 'i') {
          as_bibyte = true;
          i++;
        }
        if (i < size_len) {
          c = size_str[i];
          if (c == 'b') {
            as_bit = true;
          } else if (c != 'B') {
            throw std::invalid_argument("invalid size string: " + size_str);
          }
        }
      }
      if (i < size_len - 1) {
        throw std::invalid_argument("invalid size string: " + size_str);
      }
      break;
    }
  }

  size_num =
      size_num * pow_ul((as_bibyte ? 1024 : 1000), scale) / (as_bit ? 8 : 1);

  return size_num;
}

}  // namespace utils