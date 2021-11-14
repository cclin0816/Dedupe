#pragma once
#include <cstddef>
#include <filesystem>
#include <string>

#include "rm_t.hpp"

namespace dedupe {

/**
 * @brief Detect duplicate file, thread-safe
 * @param dir path to target directory
 * @param rm_meth remove method when found duplicate
 * @param num_thread thread count of thread pool
 * @param mem_lim total memory limit ex. 4GB, 16MiB
 * @param hash_algo digest algorithm supported by libcrypto (default md5)
 * @param log_path log file path (null string logs to stdout)
 */
void dedupe(const std::filesystem::path dir, const rm_t rm_meth,
            const std::size_t num_thread = 4,
            const std::string mem_lim = "1GiB",
            const std::string hash_algo = "md5",
            const std::string log_path = "");

}  // namespace dedupe
