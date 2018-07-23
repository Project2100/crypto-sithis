cmake_minimum_required(VERSION 3.10)
project(crypto-sithis)

set(CMAKE_VERBOSE_MAKEFILE=1)

if (UNIX)
    find_package(Threads REQUIRED)
endif (UNIX)

add_library(crypto-os STATIC plat.h default.h proto.h error.c log.c file.c net.c multi.c sync.c pool.c string.c string_heap.c filewalker.c arguments.c list.c)

add_executable(server serv.c)
add_executable(client client.c)
add_executable(crypto crypto.c)

add_dependencies(server crypto)

target_link_libraries(server crypto-os)
target_link_libraries(client crypto-os)
target_link_libraries(crypto crypto-os)

if (UNIX)
    target_link_libraries(crypto-os Threads::Threads)
endif (UNIX)

if (WIN32)
    target_link_libraries(crypto-os ws2_32)
endif (WIN32)