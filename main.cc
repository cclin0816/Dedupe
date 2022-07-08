#include <filesystem>
#include <vector>

#include "dedupe.hh"

int main(int argc, char* argv[]) {
  std::vector<std::filesystem::path> search_dir{argv + 1, argv + argc};
  auto dupe_list = dedupe::dedupe(search_dir, 8);
}
