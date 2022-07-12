#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "config.hh"

// int main() {
//   std::string path;
//   while (true) {
//     path.clear();
//     std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\"');
//     if (std::cin.eof()) {
//       break;
//     }
//     // start of path
//     char buf[4096];
//     while (true) {
//       std::cin.getline(buf, sizeof(buf), '\"');
//       if (std::cin.eof()) {
//         std::cerr << "missing end of path" << std::endl;
//         return 1;
//       }
//       auto len = std::cin.gcount() - 1;
//       path += std::string_view(buf, len);
//       // check if escaped
//       auto back_slash_count = 0UL;
//       for (auto itr = path.rbegin(); itr != path.rend(); ++itr) {
//         if (*itr == '\\') {
//           ++back_slash_count;
//         } else {
//           break;
//         }
//       }
//       if (back_slash_count % 2 == 0) {
//         // end of path
//         break;
//       } else {
//         // escaped
//         path += '\"';
//       }
//     }
//     // normalize path
//     std::string normalized_path;
//     normalized_path.reserve(path.size());
//     bool escape = false;
//     for (auto &c : path) {
//       if (c == '\\' && !escape) {
//         escape = true;
//       } else {
//         escape = false;
//         normalized_path += c;
//       }
//     }
//     std::error_code ec;
//     if (!std::filesystem::remove(normalized_path, ec) || ec) {
//       std::cerr << "failed to remove: "
//                 << std::filesystem::path(normalized_path) << " - "
//                 << ec.message() << std::endl;
//       return 1;
//     }
//   }
// }

namespace dedupe {

inline namespace detail_v1_0_0 {

void DEDUPE_EXPORT remove(const std::vector<std::filesystem::path> &rm_list) {
  for (const auto &path : rm_list) {
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) || ec) {
      std::string msg = ec ? ec.message() : "doesn't exist";
      std::cerr << "[err] failed to remove: " << path << " - " << msg
                << std::endl;
    }
  }
}

}  // namespace detail_v1_0_0

}  // namespace dedupe