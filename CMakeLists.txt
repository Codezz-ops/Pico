cmake_minimum_required(VERSION 3.27)
project(Cedit C)

include(cmake/CompilerSettings.cmake)

add_executable(main src/main.c src/editor/editor.c src/append/append.c src/errors/die.c)

target_link_libraries(main rt)