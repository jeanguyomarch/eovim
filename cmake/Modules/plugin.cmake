set(EOVIM_INCLUDE_DIRS
   "${CMAKE_SOURCE_DIR}/include"
   "${CMAKE_BINARY_DIR}/include")

function (add_plugin Plugin)
   set(SOURCES "${CMAKE_SOURCE_DIR}/plugins/${Plugin}/plugin.c")
   add_library(${Plugin} MODULE
      ${SOURCES})
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
