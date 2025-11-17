cmake_minimum_required(VERSION 4.2)

include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)



project(main)
add_executable(main ./src/main.cpp)

add_compile_definitions(LIBSSH_STATIC=1)
find_package(LIBSSH REQUIRED)
target_link_libraries(main ssh)



project(injector)
add_executable(injector ./src/injector.cpp)


# for building without main and injector
project(payload)
add_executable(payload ./payload/main_override.cpp)

add_compile_definitions(LIBSSH_STATIC=1)
find_package(LIBSSH REQUIRED)
target_link_libraries(payload ssh)
