# - Try to find MuPDF
# Once done this will define
#
#  MUPDF_FOUND - system has MuPDF
#  MUPDF_INCLUDE_DIR - The include directory for MuPDF headers
#  MUPDF_LIBRARIES - Link these to use MuPDF
#
# Redistribution and use is allowed according to the terms of the BSD license.
#

if ( MUPDF_LIBRARIES )
   # in cache already
   SET(MuPDF_FIND_QUIETLY TRUE)
endif ( MUPDF_LIBRARIES )

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
if( NOT WIN32 )
  find_package(PkgConfig)

  pkg_check_modules(MUPDF_PKG QUIET mupdf)
endif( NOT WIN32 )


# Find libmupdf, libfitz and associated header files (Required)
FIND_LIBRARY(MUPDF_LIBRARY NAMES mupdf
  PATHS
    /usr/local
    /usr
  HINTS
    ${MUPDF_PKG_LIBRARY_DIRS} # Generated by pkg-config
  PATH_SUFFIXES
    lib64
    lib
)

FIND_LIBRARY(FITZ_LIBRARY NAMES fitz
  PATHS
    /usr/local
    /usr
  HINTS
    ${MUPDF_PKG_LIBRARY_DIRS} # Generated by pkg-config
  PATH_SUFFIXES
    lib64
    lib
)
MARK_AS_ADVANCED(MUPDF_LIBRARY)
MARK_AS_ADVANCED(FITZ_LIBRARY)
IF ( NOT(MUPDF_LIBRARY) OR NOT(FITZ_LIBRARY) )
  MESSAGE(STATUS "Could not find MuPDF libraries." )
ELSE()
  SET(MUPDF_LIBRARIES ${MUPDF_LIBRARY} ${FITZ_LIBRARY})
ENDIF()


FIND_PATH(MUPDF_INCLUDE_DIR NAMES mupdf.h
  PATHS
    /usr/local/include
    /usr/include
  HINTS
    ${MUPDF_PKG_INCLUDE_DIRS} # Generated by pkg-config
)
IF ( NOT(MUPDF_INCLUDE_DIR) )
  MESSAGE(STATUS "Could not find MuPDF headers." )
ENDIF ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MuPDF DEFAULT_MSG MUPDF_LIBRARIES MUPDF_INCLUDE_DIR )


# show the MUPDF_(XPDF/QT4)_INCLUDE_DIR and MUPDF_LIBRARIES variables only in the advanced view
MARK_AS_ADVANCED(MUPDF_INCLUDE_DIR MUPDF_LIBRARIES)

