function (add_nazi_compiler_warnings Target)
  if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
     target_compile_options(${Target}
        PRIVATE
        -Weverything
        -Wno-reserved-id-macro
        -Wno-padded
        -Wno-gnu-empty-initializer
        -Wno-gnu-flexible-array-initializer
        -Wno-pedantic
        -Wno-assign-enum
        -Wno-bad-function-cast
     )
  elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
     target_compile_options(${Target}
        PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Winit-self
        -Wfloat-equal
        -Wtrigraphs
        -Wconversion
        -Wcast-align
        -Wlogical-op
        -Wunsafe-loop-optimizations
        -Wdouble-promotion
        -Wformat=2
     )
  endif ()

  set_property(
     TARGET ${Target}
     PROPERTY C_STANDARD 11
  )
endfunction ()
