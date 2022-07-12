#include "dedupe_same_sz.hh"

#include <algorithm>
#include <utility>

#include "config.hh"
#include "file_cmp.hh"

namespace dedupe {

inline namespace detail_v1_0_0 {

void dedupe_same_sz(std::span<file_entry_t> file_list,
                    std::vector<std::vector<std::filesystem::path>> &dupe_list,
                    std::mutex &mtx) {
  // gernerate comparer for file list
  std::vector<file_cmp_t> file_cmp_list;
  file_cmp_list.reserve(file_list.size());
  uint32_t max_hash =
      (uint32_t)log2_ceil(div_ceil(file_list[0].size(), hash_blk_sz)) + 1;
  for (auto &file : file_list) {
    file_cmp_list.emplace_back(std::move(file), max_hash);
  }

  // detect duplicates
  // sort file by hash
  std::sort(file_cmp_list.begin(), file_cmp_list.end());
  std::vector<std::vector<std::filesystem::path>> dupe_list_tmp;
  {
    // finding union of same file hash
    auto union_st = file_cmp_list.begin();
    auto union_ed = union_st + 1;
    while (true) {
      if (union_ed == file_cmp_list.end() || *union_ed != *union_st) {
        // end of union
        auto union_sz = std::distance(union_st, union_ed);
        if (union_sz > 1) {
          // union size > 1, duplicates found
          auto &group = dupe_list_tmp.emplace_back();
          group.reserve((uint64_t)union_sz);
          for (; union_st != union_ed; ++union_st) {
            group.emplace_back(union_st->path());
          }
        }
        if (union_ed == file_cmp_list.end()) {
          break;
        }
        union_st = union_ed;
      }
      ++union_ed;
    }
  }

  // append to global list
  if (!dupe_list_tmp.empty()) {
    std::lock_guard lk(mtx);
    dupe_list.insert(dupe_list.end(),
                     std::make_move_iterator(dupe_list_tmp.begin()),
                     std::make_move_iterator(dupe_list_tmp.end()));
  }
}

}  // namespace detail_v1_0_0

}  // namespace dedupe