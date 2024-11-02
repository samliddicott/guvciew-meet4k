set(CMAKE_C_STANDARD 11)

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.1")

add_library(gviewencoder SHARED
  audio_codecs.c
  avi.c
  encoder.c
  file_io.c
  libav_encoder.c
  packet.c
  matroska.c
  muxer.c
  stream_io.c
  video_codecs.c
)

set_target_properties(
  gviewencoder PROPERTIES
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavutil)

target_link_libraries(gviewencoder ${FFMPEG_LIBRARIES})
add_definitions(${FFMPEG_CFLAGS} ${FFMPEG_CFLAGS_OTHER})

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewencoder.pc.in libgviewencoder.pc @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gviewencoder.h TYPE INCLUDE)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libgviewencoder.pc
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewencoder)
