#include <getopt.h>

#include <thread>

#include "dedupe.hpp"

int main() {
  dedupe::dedupe("/mnt/d", dedupe::rm_t::log,
                 std::thread::hardware_concurrency(), "4GB");
}