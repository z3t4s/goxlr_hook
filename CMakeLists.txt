cmake_minimum_required(VERSION 3.14.5)
set(CMAKE_GENERATOR_PLATFORM win32)

project(goxlr_hook)

# Visual Studio generator specific flags
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options(/MP)
endif()

#Set default C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

# Enable solution folder support
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Workaround for https://github.com/cpm-cmake/CPM.cmake/issues/281
set(CPM_SOURCE_CACHE OFF CACHE BOOL "" FORCE)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Project target
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp *.hpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCES})

add_subdirectory("minhook")
target_link_libraries(${PROJECT_NAME}
    minhook    
)

# Set Visual Studio startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
