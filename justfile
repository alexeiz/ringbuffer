conan-install:
    conan install . --output-folder=build/debug --build=missing -s build_type=Debug
    conan install . --output-folder=build/release --build=missing -s build_type=Release
