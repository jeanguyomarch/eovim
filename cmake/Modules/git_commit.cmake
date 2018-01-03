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
         OUTPUT_VARIABLE   GIT_SHORT_HASH
         RESULT_VARIABLE   GIT_STATUS
         ERROR_QUIET)
      if (GIT_STATUS EQUAL 0)
         # Git adds a newline, that needs to be nuked.
         # XXX Windows?? How about your godammit \r\n?
         string(REPLACE "\n" "" GIT_SHORT_HASH "${GIT_SHORT_HASH}")
         set(EOVIM_GIT_HASH "${GIT_SHORT_HASH}")
      endif ()

   endif ()
endif ()
