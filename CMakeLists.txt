cmake_minimum_required(VERSION 3.20)
project(hdrchain LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hdrchain_lib STATIC
  src/sha256.cpp src/sha256.hpp
  src/header.cpp src/header.hpp
  src/chain.cpp src/chain.hpp)
target_include_directories(hdrchain_lib PUBLIC src)

add_executable(hdrchain src/main.cpp)
target_link_libraries(hdrchain PRIVATE hdrchain_lib)

add_executable(hdrchain_tests tests/test_hdrchain.cpp)
target_link_libraries(hdrchain_tests PRIVATE hdrchain_lib)

if (MSVC)
  target_compile_options(hdrchain_lib PRIVATE /W4 /permissive- /utf-8)
  target_compile_options(hdrchain PRIVATE /W4 /permissive- /utf-8)
  target_compile_options(hdrchain_tests PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(hdrchain_lib PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(hdrchain PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(hdrchain_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME hdrchain_tests COMMAND hdrchain_tests)
