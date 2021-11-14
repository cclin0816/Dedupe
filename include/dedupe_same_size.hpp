#pragma once

#include <openssl/evp.h>

#include <atomic>
#include <cstddef>
#include <iostream>

#include "df_t.hpp"
#include "file_entry_t.hpp"
#include "rm_t.hpp"

namespace dedupe {

/**
 * @param files file entries of same file size
 */
void dedupe_same_size(const file_entry_vec files, const EVP_MD *hash_type,
                      std::atomic<std::size_t> &dup_cnt,
                      std::atomic<std::size_t> &dup_total_size,
                      std::ostream &log_stream, const rm_t rm_meth,
                      const df_t df_meth);

}  // namespace dedupe