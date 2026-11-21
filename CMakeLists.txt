cmake_minimum_required(VERSION 3.20)
project(nexstream_query VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(NEXSTREAM_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)
include(CTest)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_library(nexstream_query_core
    src/domain/query_model.cpp
    src/application/query_service.cpp
    src/infrastructure/clickhouse_client.cpp
    src/infrastructure/http_server.cpp)
target_include_directories(nexstream_query_core PUBLIC include)
target_link_libraries(nexstream_query_core PUBLIC CURL::libcurl Threads::Threads)
target_compile_options(nexstream_query_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(NEXSTREAM_ENABLE_SANITIZERS)
  target_compile_options(nexstream_query_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(nexstream_query_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(nexstream-query src/bootstrap/main.cpp)
target_link_libraries(nexstream-query PRIVATE nexstream_query_core)

if(BUILD_TESTING)
  add_executable(nexstream_query_tests tests/unit/query_tests.cpp)
  target_link_libraries(nexstream_query_tests PRIVATE nexstream_query_core)
  add_test(NAME nexstream_query_tests COMMAND nexstream_query_tests)
endif()
