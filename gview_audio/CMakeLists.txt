set(CMAKE_C_STANDARD 11)

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.0")

option(USE_PULSE "Enable Pulseaudio API" ON)

add_library(gviewaudio SHARED
  audio.c
  audio_fx.c
  audio_portaudio.c
  core_time.c
)

set_target_properties(
  gviewaudio PROPERTIES
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)

pkg_check_modules(PORTAUDIO2 REQUIRED portaudio-2.0)

target_link_libraries(gviewaudio ${PORTAUDIO2_LIBRARIES})
add_definitions(${PORTAUDIO2_CFLAGS} ${PORTAUDIO2_CFLAGS_OTHER})

if(USE_PULSE)
  pkg_check_modules(PULSEAUDIO libpulse)
  if(PULSEAUDIO_FOUND)
    message(STATUS "pulseaudio is ON")
    target_sources(gviewaudio PRIVATE audio_pulseaudio.c)
    add_compile_definitions(HAS_PULSEAUDIO)
    target_link_libraries(gviewaudio ${PULSEAUDIO_LIBRARIES})
    add_definitions(${PULSEAUDIO_CFLAGS} ${PULSEAUDIO_CFLAGS_OTHER})
  endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewaudio.pc.in libgviewaudio.pc @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gviewaudio.h TYPE INCLUDE)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libgviewaudio.pc
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewaudio)
