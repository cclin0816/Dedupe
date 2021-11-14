#include "dedupe_same_size.hpp"

#include "file_t.hpp"
#include "rm_file.hpp"

namespace dedupe {

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

}  // namespace dedupe