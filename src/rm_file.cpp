#include "rm_file.hpp"

#include <exception>
#include <filesystem>
#include <syncstream>

namespace dedupe {

namespace fs = std::filesystem;

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
      default:
        throw std::logic_error("rm_file: rm_meth is invalid");
        break;
    }
  } catch (std::exception &e) {
    std::osyncstream(std::cerr) << "Error removing: " << dup_path << '\n'
                                << e.what() << std::endl;
  }
}

}  // namespace dedupe