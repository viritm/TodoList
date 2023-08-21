

set(SOURCE_FILES main.cpp)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Создание исполняемого файла TodoList
add_executable(TodoList ${SOURCE_FILES})


add_subdirectory(imgui_directx12)

target_link_libraries(TodoList PRIVATE unofficial::sqlite3::sqlite3)

target_link_libraries(TodoList PUBLIC imgui_directx12)

target_link_libraries(TodoList PUBLIC d3d12.lib dxgi.lib)
