set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_GTK3 "Enable Gtk3 gui" ON)
option(USE_QT6  "Enable Qt6 gui" OFF)

add_compile_definitions(GETTEXT_PACKAGE="guvcview")
add_compile_definitions(PACKAGE_DATA_DIR="${CMAKE_INSTALL_DATADIR}")
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

add_executable(guvcview
  config.c
  core_io.c
  gui.c
  guvcview.c
  options.c
  video_capture.c
)

add_definitions("-Wall")

if(USE_GTK3)
  pkg_check_modules(GTK3 gtk+-3.0)
  if(GTK3_FOUND)
    message(STATUS "Gtk3 is ON")
    add_compile_definitions(ENABLE_GTK3)
    target_link_libraries(guvcview ${GTK3_LIBRARIES})
    add_definitions(${GTK3_CFLAGS} ${GTK3_CFLAGS_OTHER})
    target_sources(guvcview PRIVATE
      gui_gtk3_audioctrls.c
      gui_gtk3.c
      gui_gtk3_callbacks.c
      gui_gtk3_h264ctrls.c
      gui_gtk3_menu.c
      gui_gtk3_v4l2ctrls.c
      gui_gtk3_videoctrls.c)
  endif()
endif()

if(USE_QT6)
  pkg_check_modules(QT6 Qt6Core Qt6Widgets Qt6Gui)
  if(QT6_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    message(STATUS "Qt6 is ON")
    qt_standard_project_setup()

    qt_add_library(guvcviewqt STATIC
      gui_qt6_audioctrls.cpp
      gui_qt6_callbacks.cpp
      gui_qt6.cpp
      gui_qt6_h264ctrls.cpp
      gui_qt6_menu.cpp
      gui_qt6_v4l2ctrls.cpp
      gui_qt6_videoctrls.cpp)

    add_compile_definitions(ENABLE_QT6)
    target_link_libraries(guvcviewqt PRIVATE Qt6::Widgets)
    add_definitions(${QT6_CFLAGS} ${QT6_CFLAGS_OTHER})

    target_link_libraries(guvcview guvcviewqt)
  endif()
endif()

if(GL_FOUND)
  message(STATUS "linking guvcview to ${GL_LIBRARIES}")
  target_link_libraries(guvcview ${GL_LIBRARIES})
endif()

include_directories(
  ${CMAKE_SOURCE_DIR}/includes
  ${CMAKE_SOURCE_DIR}/gview_audio
  ${CMAKE_SOURCE_DIR}/gview_encoder
  ${CMAKE_SOURCE_DIR}/gview_render
  ${CMAKE_SOURCE_DIR}/gview_v4l2core)

target_link_libraries(guvcview
  gviewaudio
  gviewencoder
  gviewrender
  gviewv4l2core)

install(TARGETS guvcview)
