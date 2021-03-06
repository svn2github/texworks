# Adapted from CMake 2.8 QT4_CREATE_TRANSLATION
# TODO: Find a better name for this

  
# Qt-version-agnostic wrappers
if (QT_VERSION_MAJOR EQUAL 5)
  macro(QT_ADD_RESOURCES)
    QT5_ADD_RESOURCES(${ARGV} OPTIONS -threshold 5 -compress 9)
  endmacro()
else()
  macro(QT_ADD_RESOURCES)
    QT4_ADD_RESOURCES(${ARGV} OPTIONS -threshold 5 -compress 9)
  endmacro()
endif()


macro (QT_EXTRACT_OPTIONS _qt_files _qt_options)
  set(${_qt_files})
  set(${_qt_options})
  set(_QT_DOING_OPTIONS FALSE)
  foreach(_currentArg ${ARGN})
    if ("${_currentArg}" STREQUAL "OPTIONS")
      set(_QT_DOING_OPTIONS TRUE)
    else ()
      if(_QT_DOING_OPTIONS)
        list(APPEND ${_qt_options} "${_currentArg}")
      else()
        list(APPEND ${_qt_files} "${_currentArg}")
      endif()
    endif ()
  endforeach()
endmacro ()



MACRO(CREATE_QT_PRO_FILE _pro_path _pro_include_path)
  QT_EXTRACT_OPTIONS(_pro_files _pro_options ${ARGN})
  SET(_my_sources)
  SET(_my_headers)
  SET(_my_forms)
  SET(_my_dirs)
  SET(_my_tsfiles)
  SET(_my_rcfile)
  SET(_ts_pro)
  GET_FILENAME_COMPONENT(_pro_basepath ${_pro_path} PATH)
  FOREACH (_file ${_pro_files})
    GET_FILENAME_COMPONENT(_ext ${_file} EXT)
    GET_FILENAME_COMPONENT(_abs_FILE ${_file} ABSOLUTE)
    IF(NOT _ext)
      LIST(APPEND _my_dirs ${_abs_FILE})
    ELSEIF(_ext MATCHES "ts")
      LIST(APPEND _my_tsfiles ${_abs_FILE})
    ELSEIF(_ext MATCHES "ui")
      LIST(APPEND _my_forms ${_abs_FILE})
    ELSEIF(_ext MATCHES ".(h|hpp|hxx)")
      LIST(APPEND _my_headers ${_abs_FILE})
    ELSEIF(_ext MATCHES "\\.(c|cpp|cxx|c\\+\\+)")
      LIST(APPEND _my_sources ${_abs_FILE})
    ELSEIF(_ext MATCHES "rc")
      IF(_my_rcfile)
        MESSAGE(AUTHOR_WARNING "CREATE_QT4_PRO_FILE got two rc files: ${_my_rcfile} and ${_abs_FILE}. Ignoring the latter.")
      ELSE()
        SET(_my_rcfile "${_abs_FILE}")
      ENDIF()
    ELSE()
      MESSAGE(AUTHOR_WARNING "CREATE_QT4_PRO_FILE cannot handle file '${_abs_FILE}'.")
    ENDIF()
  ENDFOREACH(_file)

  # Construct the .pro file
  # It includes SOURCES and TRANSLATIONS settings, but nothing else.
  SET(_pro_content "# WARNING: This file was generated automatically by CMake.\n\n")
  SET(_pro_content "${_pro_content}error(\"This file is not intended for building ${PROJECT_NAME}. Please use CMake instead. See the README for further instructions.\")\n\n")
  SET(_pro_content "${_pro_content}# INCLUDEPATH must be set so lupdate finds headers, namespace declarations, etc.\n")
  SET(_pro_content "${_pro_content}INCLUDEPATH += ${_pro_include_path}")
  IF(_my_sources)
    SET(_pro_content "${_pro_content}\n\nSOURCES =")
    FOREACH(_pro_file ${_my_sources})
      FILE(RELATIVE_PATH _pro_file ${_pro_basepath} ${_pro_file})
      SET(_pro_content "${_pro_content} \\\n  \"${_pro_file}\"")
    ENDFOREACH(_pro_file ${_my_sources})
  ENDIF(_my_sources)
  IF(_my_headers)
    SET(_pro_content "${_pro_content}\n\nHEADERS =")
    FOREACH(_pro_file ${_my_headers})
      FILE(RELATIVE_PATH _pro_file ${_pro_basepath} ${_pro_file})
      SET(_pro_content "${_pro_content} \\\n  \"${_pro_file}\"")
    ENDFOREACH(_pro_file ${_my_headers})
  ENDIF(_my_headers)
  IF(_my_forms)
    SET(_pro_content "${_pro_content}\n\nFORMS =")
    FOREACH(_pro_file ${_my_forms})
      FILE(RELATIVE_PATH _pro_file ${_pro_basepath} ${_pro_file})
      SET(_pro_content "${_pro_content} \\\n  \"${_pro_file}\"")
    ENDFOREACH(_pro_file ${_my_forms})
  ENDIF(_my_forms)
  IF(_my_tsfiles)
    SET(_pro_content "${_pro_content}\n\nTRANSLATIONS =")
    FOREACH(_pro_file ${_my_tsfiles})
      FILE(RELATIVE_PATH _pro_file ${_pro_basepath} ${_pro_file})
      SET(_pro_content "${_pro_content} \\\n  \"${_pro_file}\"")
    ENDFOREACH(_pro_file ${_my_tsfiles})
  ENDIF(_my_tsfiles)
  IF(_my_rcfile)
    FILE(RELATIVE_PATH _pro_file ${_pro_basepath} ${_my_rcfile})
    SET(_pro_content "${_pro_content}\n\nRC_FILE = \"${_pro_file}\"")
  ENDIF(_my_rcfile)
  FILE(WRITE ${_pro_path} "${_pro_content}\n")
ENDMACRO(CREATE_QT_PRO_FILE)

# QT_ADD_QM_TRANSLATIONS(<output_var> <1.qm> [<2.qm> ...])
MACRO(QT_ADD_QM_TRANSLATIONS _qm_res)
  # Construct an appropriate resource file
  SET(_qm_qrc "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n<qresource>\n")
  FOREACH(_qm_file ${ARGN})
    FILE(RELATIVE_PATH _qm_file ${CMAKE_CURRENT_BINARY_DIR} ${_qm_file})
    GET_FILENAME_COMPONENT(_qm_filename ${_qm_file} NAME)
    SET(_qm_qrc "${_qm_qrc}<file alias=\"resfiles/translations/${_qm_filename}\">${_qm_file}</file>\n")
  ENDFOREACH(_qm_file ${_qm_files})
  SET(_qm_qrc "${_qm_qrc}</qresource>\n</RCC>\n")
  SET(_qm_qrc_path ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_trans.qrc)
  FILE(WRITE ${_qm_qrc_path} ${_qm_qrc})

  QT_ADD_RESOURCES(${_qm_res} ${_qm_qrc_path})
ENDMACRO(QT_ADD_QM_TRANSLATIONS)
