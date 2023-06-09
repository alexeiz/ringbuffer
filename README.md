# ringbuffer

## Configure/build

```
$ mkdir build
$ conan install . --output-folder=build --build=missing -s build_type=Debug
$ cd build
$ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE:FILEPATH=conan_toolchain.cmake -S.. -B. -G Ninja
$ cmake --build .
```
