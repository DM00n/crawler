cmake_minimum_required(VERSION 3.15)
project(crawler)

set(CMAKE_CXX_STANDARD 20)

file(GLOB CPPS "*.cpp" "*.h" "*.c" ".hpp")

add_executable(crawler main.cpp ${CPPS} source.cpp header.h root_certificate.h)

find_package (Threads REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(crawler
        ${CMAKE_THREAD_LIBS_INIT}
        )

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS filesystem program_options)

target_link_libraries(crawler Boost::filesystem Boost::program_options OpenSSL::SSL)