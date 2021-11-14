#include <cstddef>
#include <string>
#include <vector>

#include "df_t.hpp"
#include "file_entry_t.hpp"

namespace dedupe {

class file_t {
 public:
  std::string path;
  std::size_t size;
  file_t(const file_entry_t &fe, const df_t _df_meth,
         const EVP_MD *_hash_type) {}
  bool is_dup(file_t &rhs) { return false; }
};

using file_vec = std::vector<file_t>;

}  // namespace dedupe