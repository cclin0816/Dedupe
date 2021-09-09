#include <getopt.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

class file_entry_t {
 public:
  std::string path = "";
  std::size_t size = 0;
  file_entry_t(auto &&_path, const std::size_t _size)
      : path(std::forward<decltype(_path)>(_path)), size(_size) {}
  file_entry_t(file_entry_t &&rhs) {
    path = std::move(rhs.path);
    size = rhs.size;
  }
  file_entry_t &operator=(file_entry_t &&rhs) {
    path = std::move(rhs.path);
    size = rhs.size;
    return *this;
  }
};

class file_buf_t {
 public:
  std::string path = "";
  char *buf = nullptr;
  std::size_t size = 0;
  file_buf_t() = delete;
  file_buf_t(const file_entry_t &fe) {
    path = fe.path;
    size = fe.size;
    if (size != 0) {
      std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
      buf = new char[size];
      ifs.read(buf, size);
      if (!ifs.good() || (ifs.peek() != EOF)) {
        std::cerr << "error reading file: " << path
                  << "\ncannot open or size differ\n";
        delete[] buf;
        buf = nullptr;
        size = 0;
      }
    }
  }
  file_buf_t(const file_buf_t &) = delete;
  file_buf_t(file_buf_t &&rhs) {
    path = std::move(rhs.path);
    buf = std::exchange(rhs.buf, nullptr);
    size = std::exchange(rhs.size, 0);
  }
  ~file_buf_t() {
    if (buf != nullptr) {
      delete[] buf;
    }
  }
  bool is_same(const file_buf_t &rhs) {
    if (size == 0 || rhs.size == 0 || size != rhs.size) {
      return false;
    }
    return (std::memcmp(buf, rhs.buf, size) == 0);
  }
};

enum class remove_method { log, remove, link };
enum class diff_method { bin_diff, hash };

namespace fs = std::filesystem;
using jobs = std::vector<std::thread>;
using file_entries_t = std::vector<file_entry_t>;
using file_bufs_t = std::vector<file_buf_t>;

std::mutex cout_mutex;

void rm_file(const std::string &path, const std::string &orig_file,
             [[maybe_unused]] const remove_method rm_mtd) {
  std::lock_guard<std::mutex> cout_lock(cout_mutex);
  std::cout << path << "\n-> " << orig_file << '\n';
}

void dedupe(const std::vector<file_entry_t> same_size_files,
            const remove_method rm_mtd, const diff_method df_mtd) {
  if (df_mtd == diff_method::bin_diff) {
    file_bufs_t original_files;
    for (auto &file_entry : same_size_files) {
      file_buf_t file_buf(file_entry);
      bool is_duplicate = false;
      std::string original_entry;
      for(auto &original_file : original_files) {
        if(file_buf.is_same(original_file)){
          is_duplicate = true;
          original_entry = original_file.path;
          break;
        }
      }
      if(is_duplicate) {
        rm_file(file_buf.path, original_entry, rm_mtd);
      } else {
        original_files.emplace_back(std::move(file_buf));
      }
    }
  }
}

int main(int argc, char *const *argv) {
  std::ios_base::sync_with_stdio(false);

  fs::path target_dir(".");
  remove_method rm_mtd = remove_method::log;
  diff_method df_mtd = diff_method::bin_diff;

  // parse arg QAQ please help
  bool parse_arg = true;
  while (parse_arg) {
    switch (getopt(argc, argv, "p:r:d:")) {
      case 'p':
        target_dir = optarg;
        break;
      case 'r': {
        std::string _rm_mtd(optarg);
        if (_rm_mtd == "log") {
          rm_mtd = remove_method::log;
        } else if (_rm_mtd == "remove") {
          rm_mtd = remove_method::remove;
        } else if (_rm_mtd == "link") {
          rm_mtd = remove_method::link;
        } else {
          std::cerr << "Invalid remove method: " << _rm_mtd << '\n';
          std::exit(-1);
        }
        break;
      }
      case 'd': {
        std::string _df_mtd(optarg);
        if (_df_mtd == "bin_diff") {
          df_mtd = diff_method::bin_diff;
        } else if (_df_mtd == "hash") {
          df_mtd = diff_method::hash;
        } else {
          std::cerr << "Invalid diff method: " << _df_mtd << '\n';
          std::exit(-1);
        }
        break;
      }
      case '?':
        std::cerr << "Invalid option: " << (char)optopt << '\n';
        std::exit(-1);
      case -1:
      default:
        parse_arg = false;
        break;
    }
  }

  file_entries_t file_entries;

  // ls -R
  std::cout << "generating file list" << std::endl;
  try {
    for (auto &file : fs::recursive_directory_iterator(target_dir)) {
      if (file.is_regular_file()) {
        file_entries.emplace_back(file.path(), fs::file_size(file));
      }
    }
  } catch (fs::filesystem_error &e) {
    std::cerr << e.what() << '\n';
    std::exit(-1);
  }

  // sort with file size
  std::cout << "sorting file list" << std::endl;
  std::sort(file_entries.begin(), file_entries.end(),
            [](auto &a, auto &b) { return a.size > b.size; });

  const auto entry_cnt = file_entries.size();
  bool prev_same_size = false;
  bool next_same_size = false;
  file_entries_t same_size_files;
  jobs dedupe_jobs;


  // detect dupe
  std::cout << "detecting duplicate" << std::endl;
  for (auto i = 0UL; i < entry_cnt; i++) {
    if (i == entry_cnt - 1) {
      next_same_size = false;
    } else {
      next_same_size = (file_entries[i].size == file_entries[i + 1].size);
    }

    if (prev_same_size || next_same_size) {
      // push same size
      same_size_files.emplace_back(std::move(file_entries[i]));
      if (!next_same_size) {
        // emit job
        dedupe_jobs.emplace_back(dedupe, std::move(same_size_files), rm_mtd,
                                 df_mtd);
      }
    }
    prev_same_size = next_same_size;
  }

  for (auto &job : dedupe_jobs) {
    job.join();
  }
}