#include <getopt.h>
#include <openssl/sha.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <thread>
#include <utility>
#include <vector>
#include <execution>

// argument options

enum class remove_method { log, remove, link };
enum class diff_method { bin_diff, hash };

// global var

std::mutex cout_mutex;
std::atomic<std::size_t> dup_cnt(0);
std::counting_semaphore<64> job_sema(0);

class file_entry_t {
 public:
  std::string path = "";
  std::size_t size = 0;
  file_entry_t(auto &&_path, const std::size_t _size) noexcept
      : path(std::forward<decltype(_path)>(_path)), size(_size) {}
  file_entry_t(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = rhs.size;
  }
  file_entry_t &operator=(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = rhs.size;
    return *this;
  }
};

class file_t {
 public:
  std::string path;
  union {
    char *buf;
    unsigned char hash[SHA256_DIGEST_LENGTH];
  } data;
  std::size_t size;
  const diff_method df_mtd;
  file_t() = delete;
  file_t(const file_entry_t &fe, const diff_method _df_mtd)
      : path(fe.path), size(fe.size), df_mtd(_df_mtd) {
    if (size != 0) {  // initialize file_t if size not zero
      if (df_mtd == diff_method::bin_diff) {
        std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
        data.buf = new char[size];
        ifs.read(data.buf, size);
        if (!ifs.good() ||
            (ifs.peek() != EOF)) {  // read file with error => size set to zero
          std::cerr << "error reading file: " << path
                    << "\ncannot open or size differ\n";
          delete[] data.buf;
          size = 0;
        }
      } else {
        SHA256_CTX sha256;
        std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
        constexpr auto buf_size = 4096UL;
        char buf[buf_size];
        bool hash_fail = false;
        hash_fail = (hash_fail || SHA256_Init(&sha256) == 0);
        auto remain_size = size;

        while (remain_size > 0) {
          auto load_size = remain_size < buf_size ? remain_size : buf_size;
          ifs.read(buf, load_size);
          hash_fail =
              (hash_fail || SHA256_Update(&sha256, buf, load_size) == 0);
          remain_size -= load_size;
        }
        hash_fail = (hash_fail || SHA256_Final(data.hash, &sha256) == 0);
        if (hash_fail || !ifs.good() ||
            (ifs.peek() != EOF)) {  // read file with error => size set to zero
          std::cerr << "error hashing file: " << path
                    << "\ncannot open or size differ or hash fail\n";
          size = 0;
        }
      }
    }
  }
  file_t(const file_t &) = delete;
  file_t(file_t &&rhs) : df_mtd(rhs.df_mtd) {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);

    if (df_mtd == diff_method::bin_diff) {
      data.buf = rhs.data.buf;
    } else {
      std::memcpy(data.hash, rhs.data.hash, SHA256_DIGEST_LENGTH);
    }
  }
  ~file_t() {
    if (df_mtd == diff_method::bin_diff && size != 0) {
      delete[] data.buf;
    }
  }
  // empty file is always not the same
  bool is_same(const file_t &rhs) {
    if (size == 0 || rhs.size == 0 || size != rhs.size) {
      return false;
    }
    if (df_mtd == diff_method::bin_diff) {
      return (std::memcmp(data.buf, rhs.data.buf, size) == 0);
    } else {
      return (std::memcmp(data.hash, rhs.data.hash, SHA256_DIGEST_LENGTH) == 0);
    }
  }
};

// type alias

namespace fs = std::filesystem;
using jobs = std::vector<std::thread>;
using file_entries_t = std::vector<file_entry_t>;
using files_t = std::vector<file_t>;

void rm_file(const std::string &path, const std::string &original_file,
             [[maybe_unused]] const remove_method rm_mtd) {
  {
    std::lock_guard<std::mutex> cout_lock(cout_mutex);
    std::cout << path << "\n-> " << original_file << '\n';
  }
  dup_cnt++;
}

void dedupe(const std::vector<file_entry_t> same_size_files,
            const remove_method rm_mtd, const diff_method df_mtd) {
  files_t original_files;
  for (auto &file_entry : same_size_files) {
    file_t file(file_entry, df_mtd);
    bool is_duplicate = false;
    std::string original_entry;
    for (auto &original_file : original_files) {
      if (file.is_same(original_file)) {
        is_duplicate = true;
        original_entry = original_file.path;
        break;
      }
    }
    if (is_duplicate) {
      rm_file(file.path, original_entry, rm_mtd);
    } else {
      original_files.emplace_back(std::move(file));
    }
  }
  job_sema.release();
}

int main(int argc, char *const *argv) {
  std::ios_base::sync_with_stdio(false);

  fs::path target_dir(".");
  remove_method rm_mtd = remove_method::log;
  diff_method df_mtd = diff_method::bin_diff;
  std::size_t max_job = 16;

  // parse arg QAQ please help
  bool parse_arg = true;
  while (parse_arg) {
    switch (getopt(argc, argv, "p:r:d:j:")) {
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
      case 'j': {
        std::size_t _max_job = atol(optarg);
        if (_max_job > 0 && _max_job <= 64) {
          max_job = _max_job;
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
  job_sema.release(max_job);
  std::cout << "running with maximum " << max_job << " thread" << std::endl;

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
  std::sort(std::execution::par_unseq, file_entries.begin(), file_entries.end(),
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
        job_sema.acquire();
        dedupe_jobs.emplace_back(dedupe, std::move(same_size_files), rm_mtd,
                                 df_mtd);
      }
    }
    prev_same_size = next_same_size;
  }

  for (auto &job : dedupe_jobs) {
    job.join();
  }
  std::cout << dup_cnt << '\n';
}