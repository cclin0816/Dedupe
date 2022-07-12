#include "ls_dir_rec.hh"

#include <boost/asio.hpp>
#include <functional>
#include <iostream>

#include "oss.hh"

namespace dedupe {

inline namespace detail_v1_0_0 {

void ls_dir_rec(const std::filesystem::path dir,
                std::vector<file_entry_t> &file_list, std::mutex &mtx,
                boost::asio::thread_pool &pool,
                const std::vector<std::regex> &exclude_regex) {
  std::vector<file_entry_t> file_list_tmp;
  try {
    for (const auto &dir_entry : std::filesystem::directory_iterator(dir)) {
      if (is_exclude(dir_entry.path(), exclude_regex)) {
        // exclude, skip
        oss(std::cerr) << "[log] exclude: " << dir_entry.path() << '\n';

      } else if (dir_entry.is_symlink()) {
        // symlink, skip
        oss(std::cerr) << "[warn] skip symlink: " << dir_entry.path() << '\n';

      } else if (dir_entry.is_directory()) {
        // directory, recursive call
        boost::asio::post(
            pool,
            std::bind(ls_dir_rec, dir_entry.path(), std::ref(file_list),
                      std::ref(mtx), std::ref(pool), std::cref(exclude_regex)));

      } else if (dir_entry.is_regular_file()) {
        // regular file, add to list
        std::error_code ec;
        auto file_size = dir_entry.file_size(ec);
        if (ec) {
          // error read file size, skip
          oss(std::cerr) << "[warn] skip file: " << dir_entry.path() << " - "
                         << ec.message() << '\n';
        } else if (file_size > 0) {
          // file size > 0, add to list
          file_list_tmp.emplace_back(dir_entry.path(), file_size);
        }

      } else {
        // other file type, skip
        oss(std::cerr) << "[warn] skip unsupport file: " << dir_entry.path()
                       << '\n';
      }
    }
  } catch (std::filesystem::filesystem_error &e) {
    // error iterate directory, skip
    oss(std::cerr) << "[warn] skip directory: " << dir << " - "
                   << e.code().message() << '\n';
  }

  // append to global list
  if (!file_list_tmp.empty()) {
    std::lock_guard lk(mtx);
    file_list.insert(file_list.end(),
                     std::make_move_iterator(file_list_tmp.begin()),
                     std::make_move_iterator(file_list_tmp.end()));
  }
}

}  // namespace detail_v1_0_0

}  // namespace dedupe