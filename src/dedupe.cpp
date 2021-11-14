#include "dedupe.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind.hpp>
#include <chrono>
#include <execution>
#include <fstream>
#include <iostream>
#include <mutex>
#include <syncstream>

#include "dedupe_same_size.hpp"
#include "df_t.hpp"
#include "file_entry_t.hpp"
#include "ls_dir_rec.hpp"
#include "parse_size.hpp"

namespace dedupe {

namespace fs = std::filesystem;
namespace ba = boost::asio;
namespace cn = std::chrono;

// bool ifs_eof(std::ifstream &ifs) noexcept { return ifs.peek() == EOF; }
// class hash_excep : public std::exception {};

void dedupe(const fs::path dir, const rm_t rm_meth,
            const std::size_t num_thread, const std::string mem_lim,
            const std::string hash_algo, const std::string log_path) {
  // check hash_algo
  const EVP_MD *hash_type = EVP_get_digestbyname(hash_algo.c_str());
  if (hash_type == nullptr) {
    std::cerr << "Invalid hash algorithm: " << hash_algo << std::endl;
    return;
  }
  // check dir
  if (!fs::is_directory(dir)) {
    std::cerr << "Invalid target directory: " << dir << std::endl;
    return;
  }
  // check log_path
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
  // check mem_lim
  std::size_t mem_quota = utils::parse_size(mem_lim) / num_thread;
  if (mem_quota < 128 * 1024) {
    std::cerr << "Require Memory at least 128KiB per thread" << std::endl;
    return;
  }
  mem_quota -= 128 * 1024;

  auto st_time = cn::steady_clock::now();

  std::clog << "Running with " << num_thread << " threads" << std::endl;

  std::clog << "Generating file list" << std::endl;
  file_entry_vec file_entries;
  {
    ba::thread_pool tp(num_thread);
    std::mutex fes_mtx;
    ba::post(tp, boost::bind(ls_dir_rec, dir, boost::ref(file_entries),
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
          if (fsz * fct > mem_quota) {
            const auto max_hash_cnt =
                std::countr_zero(std::bit_ceil((fsz + 4095UL) / 4096UL)) + 1;
            if (max_hash_cnt * EVP_MAX_MD_SIZE * fct > mem_quota) {
              df_meth = df_t::no_buffer;
            } else {
              df_meth = df_t::hash;
            }
          }
          // emit job
          ba::post(tp, boost::bind(dedupe_same_size, std::move(same_size_files),
                                   hash_type, boost::ref(dup_cnt),
                                   boost::ref(dup_total_size),
                                   boost::ref(*log_stream), rm_meth, df_meth));
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

}  // namespace dedupe