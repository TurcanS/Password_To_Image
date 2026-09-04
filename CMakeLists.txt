cmake_minimum_required(VERSION 3.16)
project(passpix VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

set(PASSPIX_CORE_SOURCES
    src/crypto_utils.cpp
    src/image_gen.cpp
    src/stego.cpp
    src/image_utils.cpp
    Include/lodepng.cpp
)

add_library(passpix_core STATIC ${PASSPIX_CORE_SOURCES})
target_include_directories(passpix_core PUBLIC src Include)
target_link_libraries(passpix_core PUBLIC PkgConfig::SODIUM)

if(MSVC)
    target_compile_options(passpix_core PRIVATE /W4)
else()
    target_compile_options(passpix_core PRIVATE
        -Wall -Wextra -Wconversion -Wshadow -pedantic -fstack-protector-strong)
    set_source_files_properties(Include/lodepng.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-conversion;-Wno-shadow")
endif()

add_executable(passpix src/main.cpp src/terminal_utils.cpp)
target_link_libraries(passpix PRIVATE passpix_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_runner
        test/catch_amalgamated.cpp
        test/test_crypto.cpp
        test/test_stego.cpp
    )
    target_link_libraries(test_runner PRIVATE passpix_core)
    add_test(NAME passpix_tests COMMAND test_runner)
    set_tests_properties(passpix_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    if(UNIX)
        add_test(NAME passpix_smoke
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh $<TARGET_FILE:passpix>)
    endif()
endif()
