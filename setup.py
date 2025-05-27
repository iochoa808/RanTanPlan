#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path
from setuptools import setup
from setuptools.command.build_py import build_py


class CMakeBuildCommand(build_py):
    """Custom build command that builds C++ components with CMake."""
    
    def run(self):
        self.build_cmake()
        super().run()
    
    def build_cmake(self):
        """Build the C++ code using CMake."""
        print("Building C++ components with CMake...")
        
        source_dir = Path(__file__).parent.absolute()
        cpp_dir = source_dir / "planmt" / "cpp"
        build_dir = cpp_dir / "build"
        bin_dir = source_dir / "planmt" / "bin"
        
        build_dir.mkdir(exist_ok=True)
        bin_dir.mkdir(exist_ok=True)
        
        cmake_args = [
            f"-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_INSTALL_PREFIX={bin_dir}",
        ]
        
        try:
            subprocess.run(["cmake"] + cmake_args + [str(cpp_dir)], cwd=build_dir, check=True)
            subprocess.run(["cmake", "--build", ".", "--config", "Release"], cwd=build_dir, check=True)
            subprocess.run(["cmake", "--install", "."], cwd=build_dir, check=True)
            print("C++ build completed successfully!")
        except subprocess.CalledProcessError as e:
            print(f"Error building C++ components: {e}")
            sys.exit(1)
        except FileNotFoundError:
            print("CMake not found. Please install CMake to build C++ components.")
            sys.exit(1)


setup(cmdclass={'build_py': CMakeBuildCommand})
