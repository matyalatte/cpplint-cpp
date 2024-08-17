project('cpplint-cpp', ['c', 'cpp'],
    meson_version: '>=0.58.0',
    default_options: [
        'buildtype=debug',              # build debug by default
        'default_library=static',       # build shared libraries by default
        'warning_level=3',              # always max warnings
        'b_pch=false',                  # we don't want precompiled headers
        'b_staticpic=true',             # use PIC even for static libraries
        'c_winlibs=',                   # we define our own Windows libraries
        'cpp_std=c++20',                # strict C++20
        'cpp_eh=sc',                    # shut the compiler up in some cases
        'cpp_winlibs=',                 # likewise as with c_winlibs
        'wrap_mode=forcefallback'       # we don't use installed libraries.
    ],
    version: '0.2.0')

cpplint_link_args = []
cpplint_c_args = []
cpplint_os = host_machine.system()
cpplint_cpu = host_machine.cpu_family()
cpplint_compiler = meson.get_compiler('c').get_id()
cpplint_is_debug = get_option('buildtype').startswith('debug')

if cpplint_os == 'windows'
    if cpplint_compiler == 'msvc'
        cpplint_link_args += [
            '/LARGEADDRESSAWARE',
            '/INCREMENTAL:NO'
        ]
        if (not cpplint_is_debug)
            # enable lto
            cpplint_c_args += ['/GL']
            cpplint_link_args += ['/OPT:ICF', '/LTCG']
        endif
    else
		# don't require shipping the MinGW-w64 DLLs
		cpplint_link_args += [
			'-static',
			'-static-libgcc',
			'-static-libstdc++',
		]
	endif
elif cpplint_os == 'darwin'
    # Set deployment targets
    languages = ['c', 'cpp']
    macosx_version_min = '-mmacosx-version-min=' + get_option('macosx_version_min')
    add_global_arguments(macosx_version_min, language: languages)
    add_global_link_arguments(macosx_version_min, language: languages)

    # Check if SDKs support universal binaries or not.
    arch = ['-arch', 'x86_64', '-arch', 'arm64']
    c_compiler = meson.get_compiler('c')
    result = c_compiler.run(
        'int main(void) { return 0; }',
        name : 'universal binary test',
        args: arch)
    if result.compiled()
        # set options for universal build
        add_global_arguments(arch, language: languages)
        add_global_link_arguments(arch, language: languages)
    else
        warning('Universal build is disabled since your SDKs do not support it.')
    endif
endif

# enable JIT compiler in pcre2
cpu_jit_supported = [ 'aarch64', 'arm', 'mips', 'mips64', 'ppc', 'ppc64', 'riscv32', 'riscv64', 's390x', 'x86', 'x86_64' ]
pcre2_jit_supported = (cpplint_cpu in cpu_jit_supported and
                       cpplint_compiler != 'tcc' and
                       cpplint_os != 'darwin')
if pcre2_jit_supported
    if cpplint_compiler == 'msvc'
        add_global_arguments('/DSUPPORT_JIT', language: ['c', 'cpp'])
    else
        add_global_arguments('-DSUPPORT_JIT', language: ['c', 'cpp'])
    endif
endif

# get pcre2
pcre2_options = [
    'grep=false',
    'test=false',
]
pcre2_dep = dependency('libpcre2-8', required: true,
                       fallback: ['pcre2', 'libpcre2_8'],
                       default_options : pcre2_options)

# get pthread
thread_dep = dependency('threads', required: true)

# set source files
cpplint_sources = [
    'src/file_linter.cpp',
    'src/cpplint_state.cpp',
    'src/options.cpp',
    'src/string_utils.cpp',
    'src/regex_utils.cpp',
    'src/line_utils.cpp',
    'src/getline.cpp',
    'src/cleanse.cpp',
    'src/states.cpp',
    'src/nest_info.cpp',
]

# main binary
cpplint_lib = library('cpplint',
    cpplint_sources,
    dependencies: [pcre2_dep, thread_dep],
    c_args: cpplint_c_args,
    cpp_args: cpplint_c_args,
    link_args: cpplint_link_args,
    install: true,
    include_directories: include_directories('./include'),
    gnu_symbol_visibility: 'hidden')

# dependency for other projects
cpplint_dep = declare_dependency(
    dependencies: pcre2_dep,
    include_directories: include_directories('./include'),
    link_with : cpplint_lib)

# main app
executable('cpplint-cpp',
    cpplint_sources + ['src/cpplint.cpp'],
    dependencies: cpplint_dep,
    c_args: cpplint_c_args,
    cpp_args: cpplint_c_args,
    link_args: cpplint_link_args,
    install : true)

# Build unit tests
if get_option('tests')
    # get gtest
    gtest_proj = subproject('gtest')
    gtest_dep = gtest_proj.get_variable('gtest_dep')
    gmock_dep = gtest_proj.get_variable('gmock_dep')

    # build tests
    subdir('tests')
endif
