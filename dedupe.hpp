#include <filesystem>
#include <string>

namespace dedupe {

namespace fs = std::filesystem;

enum class rm_t {
  log,       // log entry
  remove,    // remove file
  soft_rel,  // replace with relative path symlink
  soft_abs,  // replace with absolute path symlink
  hard       // replace with hard link
};

/**
 * detect duplicate file, thread-safe
 * @param dir path to target directory
 * @param rm_meth remove method when found duplicate
 * @param num_thread thread count of thread pool
 * @param mem_lim total memory limit ex. 4GB, 16MiB
 * @param hash_algo digest algorithm supported by libcrypto (default md5)
 * @param log_path log file (if empty string than log to stdout)
 */
void dedupe(const fs::path dir, const rm_t rm_meth,
            const std::size_t num_thread, const std::string mem_lim,
            const std::string hash_algo = "md5",
            const std::string log_path = "");

}  // namespace dedupe