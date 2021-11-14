#pragma once
#include <cstddef>
#include <string>

namespace utils {

/**
 * @brief Parse a size string into a size_t.
 * @throws std::invalid_argument if not a valid size string.
 */
std::size_t parse_size(const std::string size_str);

}  // namespace utils