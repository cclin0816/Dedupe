#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

using namespace std::literals;

int main() {
  std::string path;
  while (true) {
    path.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\"');
    if (std::cin.eof()) {
      break;
    }
    // start of path
    char buf[4096];
    while (true) {
      std::cin.getline(buf, sizeof(buf), '\"');
      if (std::cin.eof()) {
        std::cerr << "missing end of path" << std::endl;
        return 1;
      }
      auto len = std::cin.gcount() - 1;
      path += std::string_view(buf, len);
      // check if escaped
      auto back_slash_count = 0UL;
      for (auto itr = path.rbegin(); itr != path.rend(); ++itr) {
        if (*itr == '\\') {
          ++back_slash_count;
        } else {
          break;
        }
      }
      if (back_slash_count % 2 == 0) {
        // end of path
        break;
      } else {
        // escaped
        path += '\"';
      }
    }
    // normalize path
    std::string normalized_path;
    normalized_path.reserve(path.size());
    bool escape = false;
    for (auto &c : path) {
      if (c == '\\' && !escape) {
        escape = true;
      } else {
        escape = false;
        normalized_path += c;
      }
    }
    std::error_code ec;
    if (!std::filesystem::remove(normalized_path, ec) || ec) {
      std::cerr << "failed to remove: "
                << std::filesystem::path(normalized_path) << " - "
                << ec.message() << std::endl;
      return 1;
    }
  }
}
