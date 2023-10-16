
set(SOURCE_FILES main.cpp)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_executable(TodoList WIN32 ${SOURCE_FILES})

add_subdirectory(imgui_directx12)

target_link_libraries(TodoList PRIVATE unofficial::sqlite3::sqlite3)

target_link_libraries(TodoList PUBLIC imgui_directx12)

target_link_libraries(TodoList PUBLIC d3d12.lib dxgi.lib)


set(INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/TodoList)


find_package(Doxygen)

if(DOXYGEN_FOUND)
    SET(DOXYGEN_INPUT ${CMAKE_SOURCE_DIR}/Doxyfile.in)
    SET(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_INPUT} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    add_custom_target(get_doc ALL 
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
    )
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif()

set(dir_src_doc ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen)

install(DIRECTORY ${dir_src_doc} DESTINATION ${INSTALL_PATH})

get_target_property(SQLITE_DLL unofficial::sqlite3::sqlite3 LOCATION)



install(TARGETS TodoList DESTINATION ${INSTALL_PATH}/bin)

install(FILES ${SQLITE_DLL} DESTINATION ${INSTALL_PATH}/bin)

