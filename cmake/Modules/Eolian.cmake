#
# A real Eolian CMake Helper!
#
find_program(EOLIAN_GEN eolian_gen)

function (add_eolian_file File)
   set(options)
   set(oneValArg DESTINATION)
   set(multiValArg DEPENDENCIES DEPENDS)
   cmake_parse_arguments(EOLIAN
      "${options}" "${oneValArg}" "${multiValArg}" ${ARGN} 
   )

   execute_process(
      COMMAND
      "${PKG_CONFIG_EXECUTABLE}" --variable=eolian_flags ${EOLIAN_DEPENDENCIES}

      OUTPUT_STRIP_TRAILING_WHITESPACE
      OUTPUT_VARIABLE EOLIAN_FLAGS
   )
   string(REPLACE " " ";" EOLIAN_FLAGS ${EOLIAN_FLAGS})

   get_filename_component(EOLIAN_FILE "${File}" NAME)

   set(gen_c "${EOLIAN_DESTINATION}/${EOLIAN_FILE}.c")
   set(gen_h "${EOLIAN_DESTINATION}/${EOLIAN_FILE}.h")
   add_custom_command(
      OUTPUT "${gen_c}"
      OUTPUT "${gen_h}"

      DEPENDS "${File}"

      COMMAND
      "${EOLIAN_GEN}" -o "c:${gen_c}" -o "h:${gen_h}" "${File}" ${EOLIAN_FLAGS}

      COMMENT "Generating sources from ${EOLIAN_FILE}"
   )
   set(target eolian-gen-${EOLIAN_FILE})
   set(${EOLIAN_OUTPUT} "${gen_c}" "${gen_h}" PARENT_SCOPE)

   add_custom_target(${target}
      DEPENDS "${gen_c}"
      DEPENDS "${gen_h}" 
   )
   add_dependencies(${EOLIAN_DEPENDS} ${target})

endfunction ()
