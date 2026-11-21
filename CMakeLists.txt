cmake_minimum_required(VERSION 3.20)
project(nexstream_query VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(NEXSTREAM_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)
include(CTest)
find_package(Threads REQUIRED)

add_library(nexstream_query_core
    src/domain/query_model.cpp)
target_include_directories(nexstream_query_core PUBLIC include)
target_link_libraries(nexstream_query_core PUBLIC Threads::Threads)
target_compile_options(nexstream_query_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(NEXSTREAM_ENABLE_SANITIZERS)
  target_compile_options(nexstream_query_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(nexstream_query_core PUBLIC -fsanitize=address,undefined)
endif()
