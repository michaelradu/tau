cmake_minimum_required(VERSION 3.27)
project(tau C)

set(CMAKE_C_STANDARD 11)

add_executable(tau main.c)
target_compile_options(tau PRIVATE -Wall -Wextra -pedantic)
