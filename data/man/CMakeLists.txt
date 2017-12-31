# Path to the default man page
set(MAN_OUT "${CMAKE_BINARY_DIR}/man/eovim.1")

# Generate the man page from the template
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/eovim.1.in" "${MAN_OUT}" @ONLY)

# Search for Gzip to compress the man page
find_program(GZIP gzip)
if (GZIP)
   add_custom_command(
      OUTPUT "${MAN_OUT}.gz"
      COMMAND "${GZIP}" -k "${MAN_OUT}")
   set(MAN_OUT "${MAN_OUT}.gz")
   add_custom_target(man-zip ALL
      DEPENDS ${MAN_OUT})
endif ()

# Install the man page
install(FILES "${MAN_OUT}" DESTINATION "share/man/man1")
