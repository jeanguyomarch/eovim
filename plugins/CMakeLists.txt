##############################################################################
# Libeovim, for plugin development
##############################################################################

add_library(libeovim SHARED
   "${CMAKE_CURRENT_SOURCE_DIR}/lib/eovim.c"
)
target_include_directories(libeovim
   SYSTEM PRIVATE
   ${EINA_INCLUDE_DIRS}
   ${ELEMENTARY_INCLUDE_DIRS}
   ${MSGPACK_INCLUDE_DIRS}
   ${EOVIM_INCLUDE_DIRS}
)
add_nazi_compiler_warnings(libeovim)
set_target_properties(libeovim PROPERTIES
   PREFIX ""
   # Place libeovim in another directory than plugins/, as in tree execution
   # will consider libeovim as a plugin, due to its .so extension on Linux
   LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
install(TARGETS libeovim LIBRARY DESTINATION lib)

##############################################################################
# Built-in plugins
##############################################################################

add_plugin(sizing)
