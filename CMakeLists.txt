# Минимальная требуемая версия CMake
cmake_minimum_required(VERSION 3.8)


# Название проекта
project(TodoList)


get_filename_component(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
add_definitions(-DPROJECT_ROOT_DIR="${PROJECT_ROOT_DIR}")



# Включение подкаталога src
add_subdirectory(src)