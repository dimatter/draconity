cmake_minimum_required(VERSION 2.8.8)

project(maclink)
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
include_directories(src)
if (ASAN)
add_definitions(-O1 -ggdb -fsanitize=address -fno-omit-frame-pointer)
else()
add_definitions(-O2)
endif()

file(GLOB_RECURSE SOURCE src/*.c)
add_library(maclink SHARED ${SOURCE})
link_directories(/Library/Developer/CommandLineTools/usr/lib/clang/8.1.0/lib/darwin/)
target_link_libraries(maclink m dl pthread zmq czmq jansson)
if (ASAN)
    target_link_libraries(maclink clang_rt.asan_osx_dynamic)
endif()
