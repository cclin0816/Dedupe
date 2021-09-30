#include "dedupe.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind.hpp>
#include <chrono>
#include <cstring>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <syncstream>
#include <utility>
#include <vector>

namespace dedupe {

namespace ba = boost::asio;
namespace cn = std::chrono;

enum class df_t { buffer, hash, no_buffer };

bool ifs_eof(std::ifstream &ifs) noexcept { return ifs.peek() == EOF; }

class hash_excep : public std::exception {};

class file_entry_t {
 public:
  std::string path;
  std::size_t size = 0;
  file_entry_t(auto &&_path, const std::size_t _size) noexcept
      : path(std::forward<decltype(_path)>(_path)), size(_size) {}
  file_entry_t(const file_entry_t &rhs) = default;
  file_entry_t(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
  }
  file_entry_t &operator=(const file_entry_t &rhs) = default;
  file_entry_t &operator=(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
    return *this;
  }
};

using file_entry_vec = std::vector<file_entry_t>;

// TODO
class file_t {
 public:
  std::string path;
  std::size_t size;
  file_t(const file_entry_t &fe, const df_t _df_meth,
         const EVP_MD *_hash_type) {}
  bool is_dup(file_t &rhs) { return false; }
};

using file_vec = std::vector<file_t>;

/** list directory recursive
 * skip open failed, skip symlink
 * @param fes[out] non empty regular files, no order guarantee
 * @param fes_mtx guard mutex for fes
 */
void list_dir_recursive(const fs::path dir, file_entry_vec &fes,
                        std::mutex &fes_mtx, ba::thread_pool &tp) {
  file_entry_vec fes_tmp;
  try {
    for (auto &dir_entry : fs::directory_iterator(dir)) {
      if (dir_entry.is_symlink()) {  // skip all symlink
        continue;
      } else if (dir_entry.is_directory()) {  // recursive call
        ba::post(tp, boost::bind(list_dir_recursive, dir_entry.path(),
                                 boost::ref(fes), boost::ref(fes_mtx),
                                 boost::ref(tp)));
      } else if (dir_entry.is_regular_file()) {  // add to local list
        try {
          std::size_t size = dir_entry.file_size();
          if (size != 0) {
            fes_tmp.emplace_back(dir_entry.path(), size);
          }
        } catch (fs::filesystem_error &e) {  // fail reading file size
          std::osyncstream(std::cerr)
              << "Skipping file: " << dir_entry.path() << std::endl;
        }
      }
    }
  } catch (fs::filesystem_error &e) {  // fail opening directory
    std::osyncstream(std::cerr) << "Skipping directory: " << dir << std::endl;
    return;
  }
  {  // append local list to main list
    std::lock_guard lk(fes_mtx);
    fes.insert(fes.end(), std::make_move_iterator(fes_tmp.begin()),
               std::make_move_iterator(fes_tmp.end()));
  }
}

void rm_file(const std::string &dup_path, const std::string &ori_path,
             const rm_t rm_meth, std::ostream &log_stream) noexcept {
  std::osyncstream(log_stream)
      << "<- " << dup_path << "\n-> " << ori_path << '\n';
  if (rm_meth == rm_t::log) return;
  try {
    fs::remove(dup_path);
    if (rm_meth == rm_t::remove) return;
    switch (rm_meth) {
      case rm_t::soft_rel: {
        fs::path _dup_path(dup_path);
        fs::create_symlink(fs::relative(ori_path, _dup_path.parent_path()),
                           _dup_path);
      } break;
      case rm_t::soft_abs:
        fs::create_symlink(fs::absolute(ori_path), dup_path);
        break;
      case rm_t::hard:
        fs::create_hard_link(ori_path, dup_path);
        break;
    }
  } catch (std::exception &e) {
    std::osyncstream(std::cerr) << "Error removing: " << dup_path << '\n'
                                << e.what() << std::endl;
  }
}

/**
 * @param files file entries of same file size
 */
void dedupe_same_size(const file_entry_vec files, const EVP_MD *hash_type,
                      std::atomic<std::size_t> &dup_cnt,
                      std::atomic<std::size_t> &dup_total_size,
                      std::ostream &log_stream, const rm_t rm_meth,
                      const df_t df_meth) {
  file_vec ori_files;
  for (auto &file_entry : files) {
    file_t file(file_entry, df_meth, hash_type);
    bool is_dup = false;
    std::string ori_entry;
    for (auto &ori_file : ori_files) {
      if (file.is_dup(ori_file)) {
        is_dup = true;
        ori_entry = ori_file.path;
        break;
      }
    }
    if (is_dup) {
      rm_file(file.path, ori_entry, rm_meth, log_stream);
      dup_cnt++;
      dup_total_size += file.size;
    } else {
      ori_files.emplace_back(std::move(file));
    }
  }
}

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

std::size_t parse_mem_lim(const std::string mem_lim) {
  std::size_t _mem_lim = stol(mem_lim);
  const auto sz = mem_lim.size();
  if (sz < 2 || _mem_lim == 0) return 0;

  const std::array<char, 5> unit({'K', 'M', 'G', 'T', 'P'});
  bool use_bit;
  bool use_i;
  std::size_t scale = 0;

  if (mem_lim[sz - 1] == 'b') {
    use_bit = true;
  } else if (mem_lim[sz - 1] != 'B') {
    return 0;
  }
  if (mem_lim[sz - 2] == 'i') {
    use_i = true;
  }
  char _unit = mem_lim[sz - 3 + (use_i ? 0 : 1)];

  for (auto i = 0UL; i < unit.size(); i++) {
    if (unit[i] == _unit) {
      scale = i + 1;
    }
  }

  _mem_lim =
      _mem_lim * pow_ul((use_i ? 1024 : 1000), scale) / (use_bit ? 8 : 1);

  return _mem_lim;
}

void dedupe(const fs::path dir, const rm_t rm_meth,
            const std::size_t num_thread, const std::string mem_lim,
            const std::string hash_algo, const std::string log_path) {
  const EVP_MD *hash_type = EVP_get_digestbyname(hash_algo.c_str());
  if (hash_type == nullptr) {
    std::cerr << "Invalid hash algorithm: " << hash_algo << std::endl;
    return;
  }
  if (!fs::is_directory(dir)) {
    std::cerr << "Invalid target directory: " << dir << std::endl;
    return;
  }
  std::ostream *log_stream = &std::cout;
  std::ofstream log_file;
  if (log_path != "") {
    log_file.open(log_path, std::ios::out | std::ios::trunc);
    if (!log_file.is_open() || !log_file.good()) {
      std::cerr << "Error opening logfile: " << log_path << std::endl;
      return;
    }
    log_stream = &log_file;
  }
  std::size_t _mem_lim = parse_mem_lim(mem_lim) / num_thread;
  if (_mem_lim < 128 * 1024) {
    std::cerr << "Require Memory at least 128KiB per thread" << std::endl;
    return;
  }

  auto st_time = cn::steady_clock::now();

  std::clog << "Running with " << num_thread << " threads" << std::endl;

  std::clog << "Generating file list" << std::endl;
  file_entry_vec file_entries;
  {
    ba::thread_pool tp(num_thread);
    std::mutex fes_mtx;
    ba::post(tp, boost::bind(list_dir_recursive, dir, boost::ref(file_entries),
                             boost::ref(fes_mtx), boost::ref(tp)));
    tp.join();
  }

  std::clog << "Sorting file list" << std::endl;
  std::sort(std::execution::par_unseq, file_entries.begin(), file_entries.end(),
            [](auto &a, auto &b) { return a.size > b.size; });

  std::clog << "Detecting duplicate" << std::endl;
  {
    const auto entry_cnt = file_entries.size();
    bool prev_same_size = false;
    bool next_same_size = false;
    file_entry_vec same_size_files;
    std::atomic<std::size_t> dup_cnt(0);
    std::atomic<std::size_t> dup_total_size(0);
    ba::thread_pool tp(num_thread);

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
          // fall back to hash when batch size too large
          df_t df_meth = df_t::buffer;
          const auto fsz = same_size_files[0].size;  // file size
          const auto fct = same_size_files.size();   // file count
          if (fsz * fct > _mem_lim) {
            if (std::countr_zero(std::bit_ceil(fsz) / 4096) * EVP_MAX_MD_SIZE *
                    fct >
                _mem_lim) {
              df_meth = df_t::no_buffer;
            } else {
              df_meth = df_t::hash;
            }
          }
          // emit job
          ba::post(
              tp,
              boost::bind(dedupe_same_size, std::move(same_size_files), rm_meth,
                          _df_meth, lk_meth, hash_type, boost::ref(dup_cnt),
                          boost::ref(dup_total_size), boost::ref(*log_stream)));
        }
      }
      prev_same_size = next_same_size;
    }
    tp.join();
    std::osyncstream(*log_stream) << std::flush;

    std::clog << "duplicate entry count: " << dup_cnt << std::endl;
    std::clog << "duplicate total size: " << dup_total_size << std::endl;
  }

  auto ed_time = cn::steady_clock::now();
  std::clog << "Elapsed time: "
            << cn::duration_cast<cn::seconds>(ed_time - st_time).count() << 's'
            << std::endl;
}
}
}  // namespace dedupe