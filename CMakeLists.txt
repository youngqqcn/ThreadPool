cmake_minimum_required(VERSION 3.12)

add_executable(main example.cpp ThreadPool.hpp)
target_link_libraries(main pthread)
