find_package(PkgConfig)

if (NOT Efl_FIND_COMPONENTS)
  message(FATAL_ERROR "COMPONENTS are mandatory")
endif ()

if (NOT (EFL_LIBRARIES AND EFL_INCLUDE_DIRS))
  foreach (mylib ${Efl_FIND_COMPONENTS})
    # Query pkg-config to retrieve information about the EFL component
    string(TOUPPER ${mylib} MYLIB)
    pkg_check_modules(${MYLIB} ${mylib})

    # If not found but REQUIRED, fail. If found, perform version checks
    # depending on whether EXACT or not was provided.
    if (${Efl_FIND_REQUIRED})
      if (NOT ${MYLIB}_FOUND)
        message(FATAL_ERROR "Failed to find EFL component '${mylib}'")
      endif ()
      if (${Efl_FIND_VERSION_EXACT})
        if (NOT ${MYLIB}_VERSION VERSION_EQUAL ${Efl_FIND_VERSION})
          message(FATAL_ERROR "Find EFL component '${mylib}' at version \
          ${${MYLIB}_VERSION}, but exacty ${Efl_FIND_VERSION} was expected")
        endif ()
      else ()
        if (NOT ${MYLIB}_VERSION VERSION_GREATER_EQUAL ${Efl_FIND_VERSION})
          message(FATAL_ERROR "Find EFL component '${mylib}' at version \
          ${${MYLIB}_VERSION}, but at least ${Efl_FIND_VERSION} was expected")
        endif ()
      endif ()
    endif ()

    list(APPEND EFL_LIBRARIES ${${MYLIB}_LIBRARIES})
    list(APPEND EFL_INCLUDE_DIRS ${${MYLIB}_INCLUDE_DIRS})
  endforeach () # <-- done searching for ${mylib}

endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Efl REQUIRED_VARS
  EFL_LIBRARIES
  EFL_INCLUDE_DIRS)
