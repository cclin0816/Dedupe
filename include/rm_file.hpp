#include <iostream>
#include <string>

#include "rm_t.hpp"

namespace dedupe {

void rm_file(const std::string &dup_path, const std::string &ori_path,
             const rm_t rm_meth, std::ostream &log_stream) noexcept;

}  // namespace dedupe
