#pragma once

namespace dedupe {

enum class rm_t {
  log,       // log entry
  remove,    // remove file
  soft_rel,  // replace with relative path symlink
  soft_abs,  // replace with absolute path symlink
  hard       // replace with hard link
};

}  // namespace dedupe