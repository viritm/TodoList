
project(imgui_lib)

set(SOURCES imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_impl_win32.cpp imgui_impl_dx12.cpp)

set(HEADERS imconfig.h imgui.h imgui_internal.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h imgui_impl_dx12.h imgui_impl_win32.h)

add_library(imgui_directx12 ${SOURCES} ${HEADERS})


target_include_directories(imgui_directx12 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})