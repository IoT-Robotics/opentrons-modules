# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(${TARGET_MODULE_NAME}
    test_main.cpp
    test_ack_cache.cpp
    test_double_buffer.cpp
    test_gcode_parse.cpp 
    test_pid.cpp
    test_thermistor_conversions.cpp
)

target_include_directories(${TARGET_MODULE_NAME} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../src  # need to add this to get the generated conversion header
)

set_target_properties(${TARGET_MODULE_NAME}
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE)

target_compile_options(${TARGET_MODULE_NAME}
    PUBLIC
    -Wall
    -Werror
    -Weffc++
    -Wreorder
    -Wsign-promo
    -Wextra-semi
    -Wctor-dtor-privacy
    -fno-rtti)

target_link_libraries(${TARGET_MODULE_NAME} 
    ${TARGET_MODULE_NAME}-core Catch2::Catch2)

catch_discover_tests(${TARGET_MODULE_NAME} )
add_build_and_test_target(${TARGET_MODULE_NAME} )
