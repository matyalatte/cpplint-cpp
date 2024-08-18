"""
Script to make or install a package.
You should put the built binary (cpplint-cpp or cpplint-cpp.exe) in ./dist
before executing setup.py.
"""

from setuptools import setup, find_packages
import sys
import platform
from packaging.tags import sys_tags

# Get architecture
arch = platform.machine().lower()
if (arch not in ['amd64', 'x86_64', 'aarch64', 'arm64']):
    # Only supports amd64 and arm64
    raise OSError(f'Unsupported CPU architecture: {arch}')

# Get system's platform tag
tag = list(sys_tags())[0].platform

# Get exe_path and plat_name
if sys.platform.startswith('linux'):
    exe_path = 'dist/cpplint-cpp'
    if ('musl' in tag):
        plat_name = tag
        print(f'Warning: Your platform is not supported officially. ({tag})')
    else:
        # Use the minimum required version of glibc
        plat_name = 'manylinux_2_29_' + arch
elif sys.platform.startswith('darwin'):
    exe_path = 'dist/cpplint-cpp'
    plat_name = 'macosx_10_15_universal2'
elif sys.platform.startswith('win32'):
    exe_path = 'dist/cpplint-cpp.exe'
    plat_name = tag
    if (arch == 'arm64'):
        print(f'Warning: Your platform is not supported officially. ({tag})')
else:
    exe_path = 'dist/cpplint-cpp'
    plat_name = tag
    print(f'Warning: Your platform is not supported officially. ({tag})')

setup(
    name='cpplint-cpp',
    version='0.2.0',
    description='C++ reimplementation of cpplint 1.7',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    author='matyalatte',
    author_email='matyalatte@gmail.com',
    url='https://github.com/matyalatte/cpplint-cpp',
    license=open('LICENSE').read(),
    include_package_data=True,
    data_files=[
        ('bin', [exe_path])
    ],
    options={
        'bdist_wheel': {
            'plat_name': plat_name,
        },
    },
)
