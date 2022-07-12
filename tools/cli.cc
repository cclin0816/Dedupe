#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "dedupe.hh"

using namespace std::literals;

int main(int argc, char* argv[]) {
  std::vector<std::filesystem::path> search_dir;
  std::vector<std::regex> exclude_regex;
  uint32_t max_thread = 8;
  bool print_out = false;

  for (int i = 1; i < argc; ++i) {
    if (argv[i] == "-i"sv) {
      ++i;
      if (i >= argc) {
        std::cerr << "missing search_dir" << std::endl;
        return 1;
      }
      search_dir.emplace_back(argv[i]);
    } else if (argv[i] == "-e"sv) {
      ++i;
      if (i >= argc) {
        std::cerr << "missing exclude_regex" << std::endl;
        return 1;
      }
      // exclude_regex.emplace_back(argv[i]);
      try {
        exclude_regex.emplace_back(argv[i]);
      } catch (const std::regex_error& e) {
        std::cerr << "invalid exclude_regex: " << argv[i] << std::endl;
        return 1;
      }
    } else if (argv[i] == "-j"sv) {
      ++i;
      if (i >= argc) {
        std::cerr << "missing max_thread" << std::endl;
        return 1;
      }
      max_thread = (uint32_t)std::stoi(argv[i]);
      if (max_thread == 0 || max_thread > 256) {
        std::cerr << "jobs must be > 0 and <= 256" << std::endl;
        return 1;
      }
    } else if (argv[i] == "-p"sv || argv[i] == "--print"sv) {
      print_out = true;
    } else if (argv[i] == "-h"sv || argv[i] == "--help"sv) {
      std::cerr << "usage: [-i search_dir] [-e exclude_regex] [-j jobs] "
                   "[-p/--print] [-h/--help]"
                << std::endl;
      return 0;
    } else {
      std::cerr << "unknown option: " << argv[i] << std::endl;
      return 1;
    }
  }

  auto dupe_list = dedupe::dedupe(search_dir, exclude_regex, max_thread);
  if (print_out) {
    for (auto& dupe : dupe_list) {
      std::cout << "----\n";
      for (auto& file : dupe) {
        std::cout << file << '\n';
      }
    }
    std::cout << "----\n";
  }
}
