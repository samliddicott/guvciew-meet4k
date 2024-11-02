set(CMAKE_C_STANDARD 11)

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.1")

add_library(gviewv4l2core SHARED
  colorspaces.c
  control_profile.c
  core_time.c
  dct.c
  frame_decoder.c
  jpeg_decoder.c
  save_image_bmp.c
  save_image.c
  save_image_jpeg.c
  save_image_png.c
  soft_autofocus.c
  uvc_h264.c
  v4l2_controls.c
  v4l2_core.c
  v4l2_devices.c
  v4l2_formats.c
  v4l2_xu_ctrls.c)

set_target_properties(
  gviewv4l2core PROPERTIES
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)

pkg_check_modules(V4L2 REQUIRED
  libv4l2 libudev libusb-1.0 libavcodec>=57.16 libavutil libpng)

target_link_libraries(gviewv4l2core ${V4L2_LIBRARIES})
add_definitions(${V4L2_CFLAGS} ${V4L2_CFLAGS_OTHER})

add_compile_definitions(GETTEXT_PACKAGE_V4L2CORE="gview_v4l2core")

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewv4l2core.pc.in libgviewv4l2core.pc @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gviewv4l2core.h TYPE INCLUDE)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libgviewv4l2core.pc
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewv4l2core)
