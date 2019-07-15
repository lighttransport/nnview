cmake_minimum_required(VERSION 3.5)

set(BUILD_TARGET "nnview")

set(BUILD_VERSION_MAJOR 0)
set(BUILD_VERSION_MINOR 1)
set(BUILD_VERSION_PATCH 0)

project(${BUILD_TARGET})

option(NNVIEW_USE_CCACHE "Compile with ccache(if available. Linux only)" OFF)
option(NNVIEW_USE_NATIVEFILEDIALOG "Use NativeFileDialog instead of ImGuiFileDialog for file browser(requires GTK3 on Linux)" ON)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include")
  message(FATAL_ERROR "The glfw submodule directory is missing! "
    "You probably did not clone submodules. It is possible to recover "
    "by running \"git submodule update --init --recursive\" on top-level directory")
endif()

# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

# We have to use C++14 since imgui-node-editor uses C++14
# (other component uses C++11)
set(CMAKE_CXX_STANDARD            14)
set(CMAKE_CXX_STANDARD_REQUIRED   YES)


find_package(OpenGL REQUIRED)
# OpenGL
include_directories(${OPENGL_INCLUDE_DIR})


# [ccache]
if (NNVIEW_USE_CCACHE)
  if (MSVC)
    # No ccache support
  else()
    find_program(CCACHE_EXE ccache)
    if (CCACHE_EXE)

      message(STATUS "Use ccache : " ${CCACHE_EXE})
      # CMAKE_C_COMPILER_LAUNCHER = available from cmake 3.4+)
      if (CMAKE_C_COMPILER_LAUNCHER)
        set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_C_COMPILER_LAUNCHER}" "${CCACHE_EXE}")
      else()
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXE}")
      endif()

      if (CMAKE_CXX_COMPILER_LAUNCHER)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}" "${CCACHE_EXE}")
      else()
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXE}")
      endif()

    endif (CCACHE_EXE)
  endif()
endif()




# [glfw]
# local glad
include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/deps")
list(APPEND UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src/glad.c)
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include)
list(APPEND EXT_LIBRARIES glfw)


# [ImGUI] and [imgui-node-editor]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Include)
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_opengl2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp

    # imgui-node-editor

    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Include/imgui_node_editor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/crude_json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/crude_json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_bezier_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_bezier_math.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_canvas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_canvas.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_extra_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_extra_math.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_node_editor_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_node_editor_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_node_editor_internal.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Source/imgui_node_editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor/NodeEditor/Support/imgui_node_editor.natvis

    )


if (NNVIEW_USE_NATIVEFILEDIALOG)
  add_definitions(-DNNVIEW_WITH_NATIVEFILEDIALOG)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/include)
  if (WIN32)
    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_win.cpp)
  elseif (APPLE)
    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_cocoa.m)
  else() # Assume linux
    # https://gist.github.com/fracek/3323924
    # Use the package PkgConfig to detect GTK+ headers/library files
    FIND_PACKAGE(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

    # Setup CMake to use GTK+, tell the compiler where to look for headers
    # and to the linker where to look for libraries
    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

    # Add other flags to the compiler
    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

    list(APPEND EXT_LIBRARIES ${GTK3_LIBRARIES})

    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_gtk.c)

  endif()

endif (NNVIEW_USE_NATIVEFILEDIALOG)

set(NNVIEW_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/nnview_app.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/io/weights-loader.cc
  )

# Increase warning level for clang.
# Only apply source files of `nnview`
# https://stackoverflow.com/questions/13638408/override-compile-flags-for-single-files
IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Assume C++ sources
  set_source_files_properties(${NNVIEW_SOURCES} PROPERTIES COMPILE_FLAGS "-Weverything -Wno-system-headers -Werror -Wno-padded -Wno-c++98-compat-pedantic -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro")
ENDIF ()

add_executable(${BUILD_TARGET}
  ${NNVIEW_SOURCES}
  ${UI_SOURCES}
)

target_include_directories(
${BUILD_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/
)

target_include_directories(${BUILD_TARGET} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include
  )

add_sanitizers(${BUILD_TARGET})

target_link_libraries(
    ${BUILD_TARGET}
    ${OPENGL_LIBRARIES}
    ${EXT_LIBRARIES}
    )

# Install the built executable into (prefix)/bin
install(TARGETS ${BUILD_TARGET} DESTINATION bin)

# [VisualStudio]
if (WIN32)
  # Set `nnview` as a startup project for VS IDE
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_TARGET})

  # For easier debugging in VS IDE(cmake 3.8.0 or later required)
  # Set working directory to $nnview git repo root.
  if (CMAKE_VERSION VERSION_GREATER 3.8.0)
	set_target_properties(${BUILD_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif ()

# ImGui Font Compressor uitility
# add_executable(imgui_font_compressor_utility ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/fonts/binary_to_compressed_c.cpp)
