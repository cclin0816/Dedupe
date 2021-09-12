#include <getopt.h>
#include <openssl/evp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

// argument options

enum class remove_method { log, remove, link };
enum class link_method { soft_rel, soft_abs, hard };
enum class diff_method { bin, hash };

namespace fs = std::filesystem;
using jobs = std::vector<std::thread>;

// global var

std::atomic<std::size_t> dup_cnt(0);
std::atomic<std::size_t> dup_size(0);
std::counting_semaphore<64> job_sema(0);
std::ostream *out_fd = &std::cout;
std::ofstream out_file;

bool ifs_good(std::ifstream &ifs) noexcept {
  return (ifs.good() && (ifs.peek() == EOF));
}

class hash_exception : public std::exception {};

class hash_ctx_t {  // exception proof EVP_MD_CTX
 public:
  EVP_MD_CTX *ctx;
  hash_ctx_t() {
    ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) [[unlikely]] {
      throw hash_exception();
    }
  }
  ~hash_ctx_t() noexcept {
    if (ctx != nullptr) [[likely]] {
      EVP_MD_CTX_free(ctx);
    }
  }
};

class file_entry_t {
 public:
  std::string path;
  std::size_t size = 0;
  file_entry_t(auto &&_path, const std::size_t _size) noexcept
      : path(std::forward<decltype(_path)>(_path)), size(_size) {}
  file_entry_t(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
  }
  file_entry_t &operator=(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
    return *this;
  }
};

struct hash_t {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int len;
  const EVP_MD *type;
};

class file_t {
 public:
  std::string path;
  union {
    char *bin;
    hash_t hash;
  };
  std::size_t size;
  const diff_method df_mtd;

  file_t() = delete;
  file_t(const file_entry_t &fe, const diff_method _df_mtd,
         const EVP_MD *_hash_type)
      : path(fe.path), size(fe.size), df_mtd(_df_mtd) {
    if (df_mtd == diff_method::bin) {
      std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
      bin = new char[size];
      ifs.read(bin, size);
      if (!ifs_good(ifs))
          [[unlikely]] {  // read file with error => size set to zero
        std::osyncstream(std::cerr) << "Error reading file: " << path << '\n';
        delete[] bin;
        size = 0;
      }
    } else {
      std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
      constexpr auto buf_size = 4096UL;
      char *buf = new char[buf_size];
      auto remain_size = size;
      hash.type = _hash_type;

      try {
        hash_ctx_t hash_ctx;
        if (EVP_DigestInit(hash_ctx.ctx, hash.type) == 0) [[unlikely]] {
          throw hash_exception();
        }

        while (remain_size > 0) {
          auto load_size = std::min(remain_size, buf_size);
          ifs.read(buf, load_size);
          if (EVP_DigestUpdate(hash_ctx.ctx, buf, load_size) == 0)
              [[unlikely]] {
            throw hash_exception();
          }
          remain_size -= load_size;
        }
        if (EVP_DigestFinal(hash_ctx.ctx, hash.digest, &(hash.len)) == 0)
            [[unlikely]] {
          throw hash_exception();
        }
        if (!ifs_good(ifs)) [[unlikely]] {
          throw hash_exception();
        }
      } catch (hash_exception) {  // hash file with error => size set to zero
        std::osyncstream(std::cerr) << "Error hashing file: " << path << '\n';
        size = 0;
      }

      delete[] buf;
    }
  }
  file_t(const file_t &) = delete;
  file_t(file_t &&rhs) noexcept : df_mtd(rhs.df_mtd) {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);

    if (df_mtd == diff_method::bin) {
      bin = rhs.bin;
    } else {
      std::memcpy(hash.digest, rhs.hash.digest, rhs.hash.len);
      hash.len = rhs.hash.len;
      hash.type = rhs.hash.type;
    }
  }
  ~file_t() noexcept {
    if (df_mtd == diff_method::bin && size != 0) {
      delete[] bin;
    }
  }
  bool is_dup(const file_t &rhs) {
    if (size == 0 || rhs.size == 0) [[unlikely]] {  // one is corrupt file_t
      return false;
    }
    if (fs::equivalent(path, rhs.path)) {  // same hardlink
      return false;
    }
    if (df_mtd == diff_method::bin) {
      return (std::memcmp(bin, rhs.bin, size) == 0);  // bin diff
    } else {
      return (std::memcmp(hash.digest, rhs.hash.digest, hash.len) ==
              0);  // hash diff
    }
  }
};

// type alias

using file_entries_t = std::vector<file_entry_t>;
using files_t = std::vector<file_t>;

void rm_file(const std::string &path, const std::string &original_file,
             const remove_method rm_mtd, const link_method lk_mtd) noexcept {
  try {
    std::osyncstream(*out_fd)
        << "<- " << path << "\n-> " << original_file << '\n';
    if (rm_mtd != remove_method::log) {
      fs::remove(path);
      if (rm_mtd == remove_method::link) {
        if (lk_mtd == link_method::soft_rel) {
          fs::create_symlink(
              fs::relative(original_file, fs::path(path).parent_path()), path);
        } else if (lk_mtd == link_method::soft_abs) {
          fs::create_symlink(fs::absolute(original_file), path);
        } else {
          fs::create_hard_link(original_file, path);
        }
      }
    }
  } catch (std::exception &e) {
    std::osyncstream(std::cerr) << "Error removing duplicate: " << path << '\n'
                                << e.what() << '\n';
  }
}

void dedupe(const std::vector<file_entry_t> same_size_files,
            const remove_method rm_mtd, const diff_method df_mtd,
            const link_method lk_mtd, const EVP_MD *hash_type) {
  files_t original_files;
  for (auto &file_entry : same_size_files) {
    file_t file(file_entry, df_mtd, hash_type);
    bool is_duplicate = false;
    std::string original_entry;
    for (auto &original_file : original_files) {
      if (file.is_dup(original_file)) {
        is_duplicate = true;
        original_entry = original_file.path;
        break;
      }
    }
    if (is_duplicate) {
      rm_file(file.path, original_entry, rm_mtd, lk_mtd);
      dup_cnt++;
      dup_size += file.size;
    } else {
      original_files.emplace_back(std::move(file));
    }
  }
  job_sema.release();
}

// parse args with getopt QAQ please help
void parse_args(int argc, char *const *argv, fs::path &target_dir,
                remove_method &rm_mtd, diff_method &df_mtd, link_method &lk_mtd,
                std::size_t &max_job, const EVP_MD *&hash_type,
                std::size_t &mem_lim) {
  bool parsing = true;
  while (parsing) {
    switch (getopt(argc, argv, "p:r:d:l:j:h:m:o:")) {
      case 'p':
        target_dir = optarg;
        break;
      case 'r': {
        if (strcmp(optarg, "log") == 0) {
          rm_mtd = remove_method::log;
        } else if (strcmp(optarg, "remove") == 0) {
          rm_mtd = remove_method::remove;
        } else if (strcmp(optarg, "link") == 0) {
          rm_mtd = remove_method::link;
        } else {
          std::cerr << "Invalid remove method: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'd': {
        if (strcmp(optarg, "bin") == 0) {
          df_mtd = diff_method::bin;
        } else if (strcmp(optarg, "hash") == 0) {
          df_mtd = diff_method::hash;
        } else {
          std::cerr << "Invalid diff method: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'l': {
        if (strcmp(optarg, "soft_rel") == 0) {
          lk_mtd = link_method::soft_rel;
        } else if (strcmp(optarg, "soft_abs") == 0) {
          lk_mtd = link_method::soft_abs;
        } else if (strcmp(optarg, "hard") == 0) {
          lk_mtd = link_method::hard;
        } else {
          std::cerr << "Invalid link method: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'j': {
        std::size_t _max_job = atol(optarg);
        if (_max_job > 0 && _max_job <= 64) {
          max_job = _max_job;
        } else {
          std::cerr << "Invalid max job: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'h': {
        const EVP_MD *_hash_type = EVP_get_digestbyname(optarg);
        if (_hash_type != nullptr) {
          hash_type = _hash_type;
        } else {
          std::cerr << "Invalid hash type: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'm': {
        std::size_t _mem_lim = atol(optarg);
        if (_mem_lim > 0) {
          mem_lim = _mem_lim;
        } else {
          std::cerr << "Invalid memory limit: " << optarg << '\n';
          std::exit(1);
        }
        break;
      }
      case 'o': {
        out_file.open(optarg, std::ios::out | std::ios::trunc);
        if(!out_file.is_open()) {
          std::cerr << "Error opening output file: " << optarg << '\n';
          std::exit(1);
        }
        out_fd = static_cast<std::ostream *> (&out_file);
        break;
      }
      case '?':
        std::cerr << "Invalid option: " << (char)optopt << '\n';
        std::exit(-1);
      case -1:
        [[fallthrough]];
      default:
        parsing = false;
        break;
    }
  }
}

int main(int argc, char *const *argv) {
  std::ios_base::sync_with_stdio(false);
  auto start_time = std::chrono::steady_clock::now();

  fs::path target_dir(".");
  remove_method rm_mtd = remove_method::log;
  link_method lk_mtd = link_method::soft_rel;
  diff_method df_mtd = diff_method::bin;
  const EVP_MD *hash_type = EVP_get_digestbyname("md5");
  std::size_t max_job = 8;
  std::size_t mem_lim = 512;  // 512 MB max per thread

  parse_args(argc, argv, target_dir, rm_mtd, df_mtd, lk_mtd, max_job, hash_type,
             mem_lim);

  file_entries_t file_entries;

  job_sema.release(max_job);
  std::clog << "\nRunning with max " << max_job << " thread" << std::endl;

  // ls -R
  std::clog << "Generating file list" << std::endl;
  try {
    for (auto &file : fs::recursive_directory_iterator(
             target_dir, fs::directory_options::skip_permission_denied)) {
      if (file.is_regular_file() && !file.is_symlink()) {
        std::size_t size = fs::file_size(file);
        if (size != 0) {
          file_entries.emplace_back(file.path(), size);
        }
      }
    }
  } catch (fs::filesystem_error &e) {
    std::cerr << e.what() << '\n';
    std::exit(1);
  }

  std::clog << "Sorting file list" << std::endl;
  std::sort(std::execution::par_unseq, file_entries.begin(), file_entries.end(),
            [](auto &a, auto &b) { return a.size > b.size; });

  const auto entry_cnt = file_entries.size();
  bool prev_same_size = false;
  bool next_same_size = false;
  file_entries_t same_size_files;
  jobs dedupe_jobs;
  std::size_t batch_total_size = 0;

  std::clog << "Detecting duplicate" << std::endl;
  for (auto i = 0UL; i < entry_cnt; i++) {
    if (i == entry_cnt - 1) {
      next_same_size = false;
    } else {
      next_same_size = (file_entries[i].size == file_entries[i + 1].size);
    }

    if (prev_same_size || next_same_size) {
      // push same size
      batch_total_size += file_entries[i].size;
      same_size_files.emplace_back(std::move(file_entries[i]));
      if (!next_same_size) {
        // emit job
        // fall back to hash when batch size too large
        diff_method _df_mtd = df_mtd;
        if (batch_total_size > mem_lim * 1024 * 1024) {
          _df_mtd = diff_method::hash;
        }
        job_sema.acquire();
        dedupe_jobs.emplace_back(dedupe, std::move(same_size_files), rm_mtd,
                                 _df_mtd, lk_mtd, hash_type);
        batch_total_size = 0;
      }
    }
    prev_same_size = next_same_size;
  }

  for (auto &job : dedupe_jobs) {
    job.join();
  }
  *out_fd << std::flush;
  std::clog << "duplicate entry count: " << dup_cnt << std::endl;
  std::clog << "duplicate total size: " << dup_size << std::endl;

  auto end_time = std::chrono::steady_clock::now();
  std::clog << "Elapsed time: "
            << std::chrono::duration_cast<std::chrono::seconds>(end_time -
                                                                start_time)
                   .count()
            << 's' << std::endl;
}