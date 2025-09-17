# ringbuffer

## Configure and build the project

Install dependencies:
```shell
conan install . --output-folder=build --build=missing -s build_type=Debug
```

Add the **default** preset to the generated **CMakeUserPresets.json** on the same level as `"include"`:
```json
    "configurePresets": [
        {
            "name": "default",
            "inherits": "conan-debug",
            "cacheVariables": {
                "CMAKE_EXPORT_COMPILE_COMMANDS": {
                    "type": "BOOL",
                    "value": "TRUE"
                }
            }
        }
    ],
    "buildPresets": [
        {
            "name": "default",
            "configurePreset": "default"
        }
    ],
    "testPresets": [
        {
            "name": "default",
            "configurePreset": "default"
        }
    ]
```

Configure and build:
```shell
cmake --preset default .
cmake --build build
```
