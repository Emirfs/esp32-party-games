cmake_minimum_required(VERSION 3.16)

# Version shown on the status page. Release builds get it from the tag (CI sets
# PG_VERSION, e.g. "1.2.0" for v1.2.0); local builds use the default below.
if(DEFINED ENV{PG_VERSION})
  set(PROJECT_VER "$ENV{PG_VERSION}")
else()
  set(PROJECT_VER "0.1.0")
endif()

# Unused, and its RGB panel driver crashes the Windows xtensa GCC 14.2 build
# (internal compiler error in esp_lcd_panel_rgb.c).
set(EXCLUDE_COMPONENTS esp_lcd)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-party-games)
