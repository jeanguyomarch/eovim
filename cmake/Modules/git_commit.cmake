# If we are in dev mode (major.minor.patch.tweak), then we will append to
# Eovim's version the short git hash of the repository. If we don't have git,
# or if git failed for some reason, too bad. The tweak version still indicates
# this is dev mode.
if (PROJECT_VERSION_TWEAK)
   find_program(GIT git)
   if (GIT)
      execute_process(
         COMMAND           "${GIT}" rev-parse --short HEAD
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
         OUTPUT_VARIABLE   EOVIM_GIT_HASH
         RESULT_VARIABLE   GIT_STATUS
         ERROR_QUIET
         OUTPUT_STRIP_TRAILING_WHITESPACE)
      if (GIT_STATUS EQUAL 0)
         message(STATUS "Short git hash: ${EOVIM_GIT_HASH}")
      endif ()
   endif ()
endif ()
