"""
Script to make or install a package.
You should copy cpplint-cpp and cpplint-cpp_info.txt
from a build directory to ./dist before executing setup.py.
"""

from setuptools import setup, find_packages
import sys
import platform
from packaging.tags import sys_tags

# Get version and platform tag
version = "0.0.0"
plat_name = ''
version_header = open('dist/version.h').readlines()
for line in version_header:
    if not line.startswith('#define'):
        continue
    name = line.split()[1]
    val = line.split()[2][1:-1]
    if name == 'CPPLINT_VERSION':
        version = val
    if name == 'PLATFORM_TAG':
        plat_name = val

if (not plat_name or plat_name == 'unknown'):
    # Get system's platform tag
    plat_name = list(sys_tags())[0].platform

# Get exe_path
if sys.platform.startswith('win32'):
    exe_path = 'dist/cpplint-cpp.exe'
else:
    exe_path = 'dist/cpplint-cpp'

setup(
    name='cpplint-cpp',
    version=version,
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
