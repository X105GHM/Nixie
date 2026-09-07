cmake_minimum_required(VERSION 3.16.0)
# Keep esp_app_desc_t.version in sync with SOFTWARE_VERSION and the OTA manifest.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/bin/version.txt")
file(READ "${CMAKE_CURRENT_LIST_DIR}/bin/version.txt" PROJECT_VER)
string(STRIP "${PROJECT_VER}" PROJECT_VER)
set(COMPONENTS src __pio_env)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Nixie_V6)
