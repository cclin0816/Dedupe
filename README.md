# Dedupe

Library for deduplicating files.

## Build

Using meson for build system

```sh=
meson setup build
meson compile -C build
```

`libdedupe.so` and `dedupe_cli` are built in the build directory.

### Requirements

* Compiler w/ C++20 support
* Meson
* Boost headers
* Ninja or other backend supported by Meson
* libxxhash

## Usage

```sh=
./dedupe_cli [-i search_dir] [-e exclude_regex] [-j jobs] [-p/--print] [-h/--help]
```

