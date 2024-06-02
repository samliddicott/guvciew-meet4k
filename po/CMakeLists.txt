include(FindGettext)

add_subdirectory(gview_v4l2core)

set(_potFile ${PROJECT_NAME}.pot)
set(_xgettext_option_list
  "--language=C"
  "--keyword=_"
  "--keyword=N_"
  "--keyword=C_:1c,2"
  "--keyword=NC_:1c,2 -s"
  "--package-name=${PROJECT_NAME}"
  "--package-version=${PRJ_VER}"
)

file(GLOB pofiles "${CMAKE_SOURCE_DIR}/po/*.po")
file(GLOB _src_list
  "${CMAKE_SOURCE_DIR}/guvcview/*.c"
  "${CMAKE_SOURCE_DIR}/guvcview/*.cpp"
)

gettext_create_translations(${_potFile} ALL ${pofiles})

find_program(XGETTEXT_EXECUTABLE xgettext)

add_custom_command(
  OUTPUT ${_potFile}
  COMMAND ${XGETTEXT_EXECUTABLE}
    ${_xgettext_option_list} -o ${_potFile} ${_src_list}
  DEPENDS ${_src_list}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Extract translatable messages to ${_potFile}"
)

add_custom_target(
  update-pot
  DEPENDS ${_potFile}
  COMMENT "Updating ${_potFile} from sources"
)

