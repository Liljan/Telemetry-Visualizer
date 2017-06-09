cmake_minimum_required(VERSION 3.6)
project(editor_plugin)

# Include common plugin CMake scripts and set the type of plugin to be to and EDITOR_PLUGIN
set(EDITOR_PLUGIN ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${REPOSITORY_DIR}/cmake")
include(CMakePlugin)

# Define automatic namespace for C++
add_compile_options(-DPLUGIN_NAMESPACE=${PROJECT_NAME})

# Scan and add project source files
find_source_files(ALL_SOURCE_FILES)

# Include editor plugin sdk files
include_directories(${REPOSITORY_DIR}/stingray_sdk)

# Setup plugin shared library
add_library(${PROJECT_NAME} SHARED ${ALL_SOURCE_FILES})

# Require Mongo package dependency.
find_package(Mongo REQUIRED)
add_compile_options(${MONGO_DEFINITIONS})
include_directories(${MONGO_INCLUDE_DIRS})
# install(FILES ${MONGO_BINARIES} DESTINATION "${ENGINE_PLUGINS_INSTALL_DIR}")
# install(DIRECTORY  ${MONGO_BINARIE_DIR} DESTINATION "${ENGINE_PLUGINS_INSTALL_DIR}")
target_link_libraries(${PROJECT_NAME} ${MONGO_LIBRARIES})

# Set target properties
set(TARGET_BASE_NAME "${PROJECT_NAME}_${ENGINE_PLUGIN_SUFFIX}_$<LOWER_CASE:$<CONFIG>>")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${TARGET_BASE_NAME}")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EDITOR_PLUGINS_INSTALL_DIR}")
