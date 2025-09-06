cmake_minimum_required(VERSION 3.16)

project(byo_redis LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # строгий -std=c++17
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WARN_FLAGS -Wall -Wextra -Werror)

add_executable(byo_redis_server
    src/blocking_io_server.cpp
)

add_executable(byo_redis_client
    src/blocking_io_client.cpp
)

add_executable(nonblocking_byo_redis_server
    src/nonblocking_io_server.cpp
)

add_executable(nonblocking_byo_redis_client
    src/nonblocking_io_client.cpp
)

target_compile_options(byo_redis_client PRIVATE ${WARN_FLAGS})
target_compile_options(byo_redis_server PRIVATE ${WARN_FLAGS})

target_compile_options(nonblocking_byo_redis_server PRIVATE ${WARN_FLAGS})
target_compile_options(nonblocking_byo_redis_client PRIVATE ${WARN_FLAGS})
