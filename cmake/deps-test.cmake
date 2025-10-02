find_package(Catch2 REQUIRED)
list(APPEND TEST_LIBRARIES Catch2::Catch2WithMain)

# CPM libraries
include("${CMAKE_CURRENT_LIST_DIR}/get_cpm.cmake")

CPMAddPackage(
    URI "gh:alexeiz/scope-exit@0.2.3"
    OPTIONS "BUILD_TESTING OFF")
list(APPEND TEST_LIBRARIES scope_exit)
