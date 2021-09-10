# Dedupe

Dedupe finds duplicate files in given directory  
weird project just to test out c++20  

## Compilation

### Requires

* `clang++>=12.0.1` or `g++>=11` or `MSVC (not tested)`
* `libcrypto>=1.1.0`

```sh=
$(CXX) -std=c++20 -O3 -Wall dedupe.cpp -o dedupe -pthread -lcrypto
```

## Execution

> ./dedupe \[-p dir\] \[-r rm_method\] \[-d diff_method\] \[-j max_thread\]

* dir - path to directory
* rm_method
  * log (default) - only print out duplicate entry
  * remove - remove and print out duplicate entry
  * link - softlink and print out duplicate entry
* diff_method
  * bin_diff (default) - use binary diff (might break if alot of same size large file)
  * hash - use file hash to compare (slower but less memory usage)
* max_thread - 1 ~ 64

## Known Issue

* should use `EVP` instead of `SHA256` for more flexible hash support
* should use thread pool instead of using semaphore controlling thread spawn
* might use up alot of memory and get killed when using `-d bin_diff`
* no hardlink support

## Not Implement Yet

* remove methods
