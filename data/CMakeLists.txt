option(USE_DEBIANMENU "Install a debian menu entry" OFF)
set(DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")

configure_file(guvcview.appdata.xml.in guvcview.appdata.xml @ONLY)
configure_file(guvcview.desktop.in guvcview.desktop @ONLY)

file(ARCHIVE_CREATE
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/guvcview.1.gz
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}/guvcview.1
  FORMAT raw
  COMPRESSION GZip
)

if(USE_DEBIANMENU)
  configure_file(guvcview.in guvcview @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/guvcview DESTINATION ${DATADIR}/menu)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/guvcview.appdata.xml DESTINATION ${DATADIR}/appdata)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/guvcview.desktop DESTINATION ${DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/guvcview.1.gz DESTINATION ${DATADIR}/man/man1)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/guvcview.png DESTINATION ${DATADIR}/pixmaps)
