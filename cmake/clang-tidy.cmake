if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES
            clang-tidy
            clang-tidy-20
            clang-tidy-19
            clang-tidy-18
            clang-tidy-17
            clang-tidy-16
            clang-tidy-15
        DOC "Path to clang-tidy executable"
    )

    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

        # Get clang-tidy version
        execute_process(
            COMMAND ${CLANG_TIDY_EXE} --version
            OUTPUT_VARIABLE CLANG_TIDY_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "clang-tidy version: ${CLANG_TIDY_VERSION}")

        # Set up clang-tidy command
        set(CLANG_TIDY_COMMAND ${CLANG_TIDY_EXE})

        # Add header filter to check headers in project
        list(APPEND CLANG_TIDY_COMMAND
            "--header-filter=${CMAKE_CURRENT_SOURCE_DIR}/.*"
        )

        # Add system header exclusion
        list(APPEND CLANG_TIDY_COMMAND
            "--system-headers=0"
        )

        # Set as CMAKE variable
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_COMMAND})

    else()
        message(WARNING "clang-tidy requested but not found!")
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    endif()
else()
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
endif()
