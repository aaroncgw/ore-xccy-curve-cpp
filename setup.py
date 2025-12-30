"""
Setup script for ore_xccy_curve_cpp Python module.

This builds the C++ library and Python bindings using CMake.
Requires: QuantLib, ORE (Open Source Risk Engine), Boost
"""

import os
import re
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name: str, sourcedir: str = "") -> None:
        super().__init__(name, sources=[])
        self.sourcedir = os.fspath(Path(sourcedir).resolve())


class CMakeBuild(build_ext):
    def build_extension(self, ext: CMakeExtension) -> None:
        ext_fullpath = Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()

        # Configuration
        cfg = "Debug" if self.debug else "Release"

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}{os.sep}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DCMAKE_BUILD_TYPE={cfg}",
            "-DBUILD_PYTHON_BINDINGS=ON",
        ]
        build_args = []

        # Check for ORE_ROOT environment variable
        ore_root = os.environ.get("ORE_ROOT")
        if ore_root:
            cmake_args.append(f"-DORE_ROOT={ore_root}")

        # Check for QUANTLIB_ROOT environment variable
        quantlib_root = os.environ.get("QUANTLIB_ROOT")
        if quantlib_root:
            cmake_args.append(f"-DQuantLib_DIR={quantlib_root}")

        # Check for BOOST_ROOT environment variable
        boost_root = os.environ.get("BOOST_ROOT")
        if boost_root:
            cmake_args.append(f"-DBOOST_ROOT={boost_root}")

        if sys.platform.startswith("win"):
            # Windows-specific configuration
            cmake_args += [
                "-A", "x64",
            ]
            build_args += ["--config", cfg]
        else:
            # Unix-specific configuration
            cmake_args += [f"-DCMAKE_BUILD_TYPE={cfg}"]

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control parallel build
        build_args += ["--", "-j4"]

        build_temp = Path(self.build_temp) / ext.name
        if not build_temp.exists():
            build_temp.mkdir(parents=True)

        subprocess.run(
            ["cmake", ext.sourcedir, *cmake_args],
            cwd=build_temp,
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", ".", *build_args],
            cwd=build_temp,
            check=True,
        )


setup(
    name="ore-xccy-curve-cpp",
    version="1.0.0",
    author="Your Name",
    author_email="your.email@example.com",
    description="C++ XCCY Curve Builder with full parameter support",
    long_description=open("README.md").read() if os.path.exists("README.md") else "",
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("ore_xccy_curve_cpp")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    python_requires=">=3.8",
)
