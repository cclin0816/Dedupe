project(
  'dedupe', 
  'cpp', 
  license : 'MIT',
  version : '1.0.0',
  default_options : [
    # core options
    'buildtype=release',
    'warning_level=3',
    'default_library=shared',
    # base options
    'b_lto=true',
    'b_lto_mode=thin', 
    'b_pie=true',
    # 'b_staticpic=true' ?
    # compiler options
    'cpp_std=c++20'
  ]
)

xxhash = dependency(
  'libxxhash', 
  version : '>=0.8.0'
)

lib_inc = include_directories('include')

lib_src = ['src/dedupe.cc', 'src/dedupe_same_sz.cc', 'src/file_cmp.cc', 'src/ls_dir_rec.cc', 'src/remove.cc']

lib = library(
  'dedupe', 
  sources : lib_src, 
  include_directories : lib_inc, 
  dependencies : [xxhash],
  cpp_args : ['-D_BOOST_ASIO_HAS_STD_INVOKE_RESULT', '-fvisibility=hidden'],
  version : '1.0.0'
)

executable('dedupe_cli', sources : 'tools/cli.cc', link_with : lib)