
message("CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")


add_executable(TodoList main.cpp)

target_compile_definitions(TodoList PRIVATE PROJECT_ROOT_DIR=${PROJECT_ROOT_DIR})

find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)


target_link_libraries(TodoList PRIVATE imgui::imgui)
target_link_libraries(TodoList PRIVATE GLEW::GLEW)
target_link_libraries(TodoList PRIVATE unofficial::sqlite3::sqlite3)
target_link_libraries(TodoList PRIVATE glfw)

set(INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/TodoList)

get_target_property(SQLITE_DLL unofficial::sqlite3::sqlite3 LOCATION)
get_target_property(GLWF_DLL glfw LOCATION)

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

    set(dir_src_doc ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen)

    install(DIRECTORY ${dir_src_doc} DESTINATION ${INSTALL_PATH})

else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif()


install(TARGETS TodoList DESTINATION ${INSTALL_PATH}/bin)

install(FILES ${SQLITE_DLL} DESTINATION ${INSTALL_PATH}/bin)
install(FILES ${GLWF_DLL} DESTINATION ${INSTALL_PATH}/bin)