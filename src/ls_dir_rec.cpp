#include "ls_dir_rec.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <iterator>
#include <syncstream>

namespace dedupe {

namespace fs = std::filesystem;
namespace ba = boost::asio;

void ls_dir_rec(const fs::path dir, file_entry_vec &fes, std::mutex &fes_mtx,
                ba::thread_pool &tp) {
  file_entry_vec fes_tmp;
  try {
    for (auto &dir_entry : fs::directory_iterator(dir)) {
      if (dir_entry.is_symlink()) {  // skip all symlink
        continue;
      } else if (dir_entry.is_directory()) {  // recursive call
        ba::post(tp, boost::bind(ls_dir_rec, dir_entry.path(), boost::ref(fes),
                                 boost::ref(fes_mtx), boost::ref(tp)));
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

}  // namespace dedupe