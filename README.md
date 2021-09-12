# Dedupe

Dedupe detects duplicate files  
weird project just to test out c++20  

## Compilation

### Requires20

* c++20 compatible compiler
  * `clang++>=12.0.1` (tested on 12.0.1 with ASAN)
  * `g++>=11` (tested on 11.2.0)
  * `MSVC>=19.29` (not tested)
* `libcrypto>=1.1.0`

```sh=
$(CXX) -std=c++20 -O2 -Wall dedupe.cpp -o dedupe -pthread -lcrypto
```

## Execution

> ./dedupe \[-p <>\] \[-r <>\] \[-d <>\] \[-j <>\] \[-h <>\] \[-l <>\] \[-m <>\] \[-o <>\]

* -p : path to directory
* -r : remove method
  * log (default) - print out duplicate entry
  * remove - remove and log
  * link - link and log
* -d : diff method
  * bin (default) - use binary diff (fall back to hash when file too large)
  * hash - use hash to compare (slower)
* -j : max thread - 1 ~ 64
* -h : hash method - any kind of digest supported by openssl
* -l : link method (ignored if not -r link)
  * soft_rel - relative symlink
  * soft_abs - absolute symlink
  * hard - hard link
* -m : memory limit of each thread (hash fall back threshold)
* -o : log output file (default stdout)

## Known Issue

* should use thread pool instead of using semaphore controlling thread spawn  
