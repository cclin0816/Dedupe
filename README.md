# Dedupe

Dedupe detect and remove duplicate files  
written in c++20  

## Compilation

### Requires

* c++20 compatible compiler
  * `clang++>=12.0.1` (tested on 12.0.1 with ASAN)
  * `g++>=11` (tested on 11.2.0)
  * `MSVC>=19.29` (not tested)
* `libcrypto>=1.1.0`

```sh=
make
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
