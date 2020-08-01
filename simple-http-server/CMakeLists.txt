cmake_minimum_required(VERSION 3.13)
project(http C)

include_directories(..)
add_executable(main main.c http.c util.c)
