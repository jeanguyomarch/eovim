set(EOVIM_INCLUDE_DIRS
   "${CMAKE_SOURCE_DIR}/include"
   "${CMAKE_BINARY_DIR}/include")

find_program(Python3 COMPONENTS Interpreter)

function (add_plugin Plugin)
   set(BUILD_DIR "${CMAKE_BINARY_DIR}/plugins")
   set(SRC_DIR "${CMAKE_SOURCE_DIR}/plugins/${Plugin}")
   set(LIB_DIR "${CMAKE_SOURCE_DIR}/plugins/lib")
   set(GEN_HELPER "${CMAKE_SOURCE_DIR}/plugins/lib/gen_helper.py")
   set(SOURCES
     "${SRC_DIR}/plugin.c"
     "${BUILD_DIR}/plugin_${Plugin}.c"
     "${BUILD_DIR}/plugin_${Plugin}.h"
   )

   add_custom_command(
      OUTPUT
        "${BUILD_DIR}/plugin_${Plugin}.c"
        "${BUILD_DIR}/plugin_${Plugin}.h"
      COMMAND
        ${Python3_EXECUTABLE} ${GEN_HELPER}
          -t "${LIB_DIR}/helper.c.j2" -o "${BUILD_DIR}/plugin_${Plugin}.c"
          -t "${LIB_DIR}/helper.h.j2" -o "${BUILD_DIR}/plugin_${Plugin}.h"
          "${SRC_DIR}/plugin.json"
      DEPENDS
        "${LIB_DIR}/helper.c.j2"
        "${LIB_DIR}/helper.h.j2"
   )
   add_library(${Plugin} MODULE
      ${SOURCES})
   target_include_directories(${Plugin}
      PRIVATE
      ${BUILD_DIR}
   )
   target_include_directories(${Plugin}
      SYSTEM PRIVATE
      ${EINA_INCLUDE_DIRS}
      ${EVAS_INCLUDE_DIRS}
      ${ELEMENTARY_INCLUDE_DIRS}
      ${MSGPACK_INCLUDE_DIRS}
      ${EOVIM_INCLUDE_DIRS}
   )
   target_link_libraries(${Plugin}
      ${EINA_LIBRARIES}
      ${EVAS_LIBRARIES}
      ${ELEMENTARY_LIBRARIES}
      ${MSGPACK_LIBRARIES}
      libeovim
   )
   add_nazi_compiler_warnings(${Plugin})
   set_target_properties(${Plugin} PROPERTIES PREFIX "")

   install(TARGETS ${Plugin} LIBRARY DESTINATION ${LIB_INSTALL_DIR}/eovim)
endfunction ()
