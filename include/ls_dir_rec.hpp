#pragma once
#include <boost/asio/thread_pool.hpp>
#include <filesystem>
#include <mutex>

#include "file_entry_t.hpp"

namespace dedupe {

/** list directory recursive
 * skip open failed, skip symlink
 * @param fes[out] non empty regular files, no order guarantee
 * @param fes_mtx guard mutex for fes
 */
void ls_dir_rec(const std::filesystem::path dir, file_entry_vec &fes,
                std::mutex &fes_mtx, boost::asio::thread_pool &tp);

}  // namespace dedupe