# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

message("FindSWI-Prolog.cmake requested version min: ${SWI-Prolog_FIND_VERSION_MIN}")
message("FindSWI-Prolog.cmake requested version max (${SWI-Prolog_FIND_VERSION_RANGE_MAX}): ${SWI-Prolog_FIND_VERSION_MAX}")

math(EXPR FIND_PLVERSION_MIN "10000*${SWI-Prolog_FIND_VERSION_MIN_MAJOR}+100*${SWI-Prolog_FIND_VERSION_MIN_MINOR}+${SWI-Prolog_FIND_VERSION_MIN_PATCH}")
math(EXPR FIND_PLVERSION_MAX "10000*${SWI-Prolog_FIND_VERSION_MAX_MAJOR}+100*${SWI-Prolog_FIND_VERSION_MAX_MINOR}+${SWI-Prolog_FIND_VERSION_MAX_PATCH}")
# message("${REQUIRED_PLVERSION_MIN}...${REQUIRED_PLVERSION_MAX}")

execute_process (
    COMMAND bash -c "swipl --dump-runtime-variables"
    OUTPUT_VARIABLE swipl_out
    RESULT_VARIABLE swipl_res
)
if(NOT swipl_res EQUAL "0")
    message("FindSWI-Prolog.cmake SWI-Prolog executable (swipl) is not in the system path")
    set (SWI-Prolog_FOUND FALSE)
else()
  # message("${swipl_out}")

  string(REGEX MATCH "PLVERSION=\"[0-9]+\";" INSTALLED_PLVERSION_LINE "${swipl_out}")
  # message("FindSWI-Prolog.cmake installed PLVERSION line: ${INSTALLED_PLVERSION_LINE}")
  string(REGEX REPLACE "^PLVERSION=\"([0-9]+)\";$" "\\1" INSTALLED_PLVERSION "${INSTALLED_PLVERSION_LINE}")
  # message("FindSWI-Prolog.cmake installed PLVERSION: ${INSTALLED_PLVERSION}")

  string(REGEX MATCH "PLBASE=\"[/A-Za-z0-9._\-]+\";" INSTALLED_PLBASE_LINE "${swipl_out}")
  # message("FindSWI-Prolog.cmake installed PLBASE line: ${INSTALLED_PLBASE_LINE}")
  string(REGEX REPLACE "^PLBASE=\"([/A-Za-z0-9._\-]+)\";$" "\\1" INSTALLED_PLBASE "${INSTALLED_PLBASE_LINE}")
  # message("FindSWI-Prolog.cmake installed PLBASE: ${INSTALLED_PLBASE}")

  string(REGEX MATCH "PLLIBDIR=\"[/A-Za-z0-9._\-]+\";" INSTALLED_PLLIBDIR_LINE "${swipl_out}")
  string(REGEX REPLACE "^PLLIBDIR=\"([/A-Za-z0-9._\-]+)\";$" "\\1" INSTALLED_PLLIBDIR "${INSTALLED_PLLIBDIR_LINE}")
  message("FindSWI-Prolog.cmake installed INSTALLED_PLLIBDIR: ${INSTALLED_PLLIBDIR}")

  string(REGEX MATCH "PLLIBSWIPL=\"[/A-Za-z0-9._\-]+\";" INSTALLED_PLLIBSWIPL_LINE "${swipl_out}")
  # message("FindSWI-Prolog.cmake installed PLLIBSWIPL line: ${INSTALLED_PLLIBSWIPL_LINE}")
  string(REGEX REPLACE "^PLLIBSWIPL=\"([/A-Za-z0-9._\-]+)\";$" "\\1" INSTALLED_PLLIBSWIPL "${INSTALLED_PLLIBSWIPL_LINE}")
  # message("FindSWI-Prolog.cmake installed PLLIBSWIPL: ${INSTALLED_PLLIBSWIPL}")

  if("${INSTALLED_PLLIBSWIPL}" STREQUAL "")
    find_library(LIBSWIPL_VIA_FIND_LIBRARY NAMES "swipl" PATHS "${INSTALLED_PLLIBDIR}" NO_CACHE)
    set (INSTALLED_PLLIBSWIPL "${LIBSWIPL_VIA_FIND_LIBRARY}")
    message("FindSWI-Prolog.cmake INSTALLED_PLLIBSWIPL: ${INSTALLED_PLLIBSWIPL}")
  endif()

  if(
    (SWI-Prolog_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE")
    AND (INSTALLED_PLVERSION GREATER_EQUAL FIND_PLVERSION_MIN)
    AND (INSTALLED_PLVERSION LESS FIND_PLVERSION_MAX)
    AND EXISTS "${INSTALLED_PLBASE}/include/SWI-Prolog.h"
  )
    message("FindSWI-Prolog.cmake the installed version (${INSTALLED_PLVERSION}) is okay")
    set (SWI-Prolog_FOUND TRUE)
    set (SWI-Prolog_BASE_DIR "${INSTALLED_PLBASE}")
    set (SWI-Prolog_INCLUDE_DIR "${INSTALLED_PLBASE}/include")
    set (SWI-Prolog_LIBRARIES "${INSTALLED_PLLIBSWIPL}")
  elseif(NOT EXISTS "${INSTALLED_PLBASE}/include/SWI-Prolog.h")
    message("FindSWI-Prolog.cmake could not find ${INSTALLED_PLBASE}/include/SWI-Prolog.h, for version ${INSTALLED_PLVERSION} \
please install SWI-Prolog devel package (in fedora is pl-devel)")
    set (SWI-Prolog_FOUND FALSE)
  else()
    message("FindSWI-Prolog.cmake installed version (${INSTALLED_PLVERSION}) is not okay")
    set (SWI-Prolog_FOUND FALSE)
  endif()
endif()
