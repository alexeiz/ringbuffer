# ringbuffer

## Configure/build

```
$ mkdir build
$ conan install . --output-folder=build --build=missing -s build_type=Debug
$ cd build
$ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE --preset conan-debug ..
$ cmake --build .
```
