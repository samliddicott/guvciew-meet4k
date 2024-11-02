set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.1")

add_library(gviewrender SHARED
  render.c
  render_fx.c
  render_osd_crosshair.c
  render_osd_vu_meter.c
)

set_target_properties(
  gviewrender PROPERTIES
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)


if(SDL2_FOUND)
  target_sources(gviewrender PRIVATE render_sdl2.c)
  target_link_libraries(gviewrender ${SDL2_LIBRARIES})
  add_definitions(${SDL2_CFLAGS} ${SDL2_CFLAGS_OTHER})
endif()

if(SFML_FOUND)
  target_sources(gviewrender PRIVATE render_sfml.cpp)
  target_link_libraries(gviewrender ${SFML_LIBRARIES})
  add_definitions(${SFML_CFLAGS} ${SFML_CFLAGS_OTHER})
endif()

if(GL_FOUND)
  message(STATUS "linking gviewrender to ${GL_LIBRARIES}")
  target_link_libraries(gviewrender ${GL_LIBRARIES})
endif()

pkg_check_modules(GSL gsl>=0.15)

if(GSL_FOUND)
  message(STATUS "gsl is ON")
  add_compile_definitions(HAS_GSL)
  target_link_libraries(gviewrender ${GSL_LIBRARIES})
  add_definitions(${GSL_CFLAGS} ${GSL_CFLAGS_OTHER})
endif()

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewrender.pc.in libgviewrender.pc @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gviewrender.h TYPE INCLUDE)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libgviewrender.pc
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewrender)
