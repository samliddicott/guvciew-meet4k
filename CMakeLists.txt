cmake_minimum_required(VERSION 3.14)
project(guvcview VERSION 2.2.1)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)

add_compile_definitions(PACKAGE_STRING="guvcview ${CMAKE_PROJECT_VERSION}")
add_compile_definitions(PACKAGE_LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

option(USE_SDL2 "Enable SDL2 render engine" ON)
option(USE_SFML "Enable SFML render engine" OFF)
option(INSTALL_DEVKIT "Install development files" OFF)

if(USE_SDL2)
  pkg_check_modules(SDL2 sdl2)
  if(SDL2_FOUND)
    message(STATUS "sdl2 is ON")
    add_compile_definitions(ENABLE_SDL2)
  endif()
endif()

if(USE_SFML)
  pkg_check_modules(SFML sfml-graphics>=2.0 sfml-window>=2.0)
  if(SFML_FOUND)
    message(STATUS "sfml is ON")
    add_compile_definitions(ENABLE_SFML)
  endif()
endif()

#set some variables need for processing
#gview libs .pc (pkgconfig) files for devkit
set(INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(LIBDIR "${CMAKE_INSTALL_LIBDIR}")
set(PREFIX "${CMAKE_INSTALL_PREFIX}")

add_subdirectory(gview_audio)
add_subdirectory(gview_encoder)
add_subdirectory(gview_render)
add_subdirectory(gview_v4l2core)
add_subdirectory(guvcview)
add_subdirectory(po)
add_subdirectory(data)

