
// #include <cstring>
// #include <openssl/sha.h>
#include <getopt.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

struct file_entry {
  std::string path;
  unsigned long size;
};

int main(int argc, char *const *argv) {
  std::ios_base::sync_with_stdio(false);

  fs::path target_dir(".");

  bool parse_arg = true;
  while (parse_arg) {
    switch (getopt(argc, argv, "p:")) {
      case 'p':
        target_dir = optarg;
        break;

      case '?':
        std::cerr << "Illegal option: " << (char)optopt << '\n';
      case -1:
      default:
        parse_arg = false;
        break;
    }
  }

  try {
    fs::current_path(target_dir);
  } catch (const fs::filesystem_error &e) {
    std::cerr << e.code().message() << '\n';
  }

  std::vector<file_entry> files;

  for (auto &p : fs::recursive_directory_iterator(target_dir)) {
    if (p.is_regular_file()) {
      // std::string path = p.path;
      files.push_back(file_entry{p.path(), fs::file_size(p)});
    }
  }

  std::sort(files.begin(), files.end(),
            [](auto &a, auto &b) { return a.size > b.size; });

  auto entry_size = files.size();

  for (auto i = 0UL; i < entry_size - 1;) {
    auto cur_file_size = files[i].size;
    if (cur_file_size == files[i + 1].size) {
      std::cout << files[i].size << '\t' << files[i].path << '\n';
      auto j = i + 1;
      while (j < entry_size) {
        if (cur_file_size == files[j].size) {
          std::cout << files[j].size << '\t' << files[j].path << '\n';
          j++;
        } else {
          break;
        }
      }
      i = j;
    } else {
      i++;
    }
  }
}