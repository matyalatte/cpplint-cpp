test_cpp_args = []

if cpplint_os == 'windows'
    if cpplint_compiler_id == 'msvc'
        test_cpp_args += ['/source-charset:utf-8']
    else
        test_cpp_args += ['-finput-charset=UTF-8']
    endif
endif

test_sources = [
    'main.cpp',
    'regex_test.cpp',
    'string_test.cpp',
    'lines_test.cpp',
    'file_test.cpp',
    'glob_test.cpp',
]

# build tests
test_exe = executable('unit_test',
    test_sources,
    dependencies : [cpplint_dep, gtest_dep, gmock_dep],
    c_args: cpplint_c_args + test_cpp_args,
    cpp_args: cpplint_c_args + test_cpp_args,
    link_args: cpplint_link_args,
    install : false)

test('unit_test', test_exe)

# copy test_files
subdir('test_files')
