
// #include <cstring>
// #include <openssl/sha.h>
#include <getopt.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;
using bytes = std::vector<char>;

struct file_entry {
  std::string path;
  unsigned long size;
};

bytes read_bin(const std::string &path) {
  std::ifstream ifs(path.c_str(), std::ios::binary | std::ios::in);
  if (!ifs.is_open()) {
    std::cerr << "bad";
    throw std::runtime_error("error opening file");
  }
  return bytes(std::istreambuf_iterator<char>(ifs), {});
}

bool bytes_cmp(const bytes &f1, const bytes &f2) {
  if (f1.size() != f2.size()) {
    return false;
  }
  const auto sz = f1.size();
  for (auto i = 0UL; i < sz; i++) {
    if (f1[i] != f2[i]) {
      return false;
    }
  }
  return true;
}

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

  std::vector<file_entry> files;
  std::vector<std::string> rm_list;

  for (auto &p : fs::recursive_directory_iterator(target_dir)) {
    if (p.is_regular_file()) {
      files.push_back(file_entry{p.path(), fs::file_size(p)});
    }
  }

  std::sort(files.begin(), files.end(),
            [](auto &a, auto &b) { return a.size > b.size; });

  const auto entry_size = files.size();

  for (auto i = 0UL; i < entry_size - 1;) {
    std::cout << i << '/' << entry_size << std::endl;
    auto cur_file_size = files[i].size;
    if (cur_file_size == files[i + 1].size) {
      auto j = i + 1;
      std::vector<bytes> original_files;
      original_files.push_back(read_bin(files[i].path));
      while (j < entry_size) {
        if (cur_file_size == files[j].size) {
          bytes f = read_bin(files[j].path);
          bool is_duplicate = false;

          for (auto &ori_f : original_files) {
            if (bytes_cmp(ori_f, f)) {
              is_duplicate = true;
              break;
            }
          }
          if (is_duplicate) {
            rm_list.push_back(files[j].path);
          } else {
            original_files.push_back(std::move(f));
          }

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

  for (auto &x : rm_list) {
    std::cout << x << '\n';
  }
}