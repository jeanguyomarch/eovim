cmake_minimum_required(VERSION 3.0.2)
project(envim LANGUAGES C VERSION 0.0.99)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(PkgConfig REQUIRED)
find_package(Eina REQUIRED)
find_package(Ecore REQUIRED)
find_package(Elementary REQUIRED)

pkg_check_modules(MSGPACK msgpack REQUIRED)

add_executable(envim
   ${SRC_DIR}/main.c
   ${SRC_DIR}/nvim.c
   ${SRC_DIR}/request.c
   ${SRC_DIR}/api.c
   ${SRC_DIR}/include/Envim.h
)
target_include_directories(envim
   SYSTEM PRIVATE
   ${EINA_INCLUDE_DIRS}
   ${ECORE_INCLUDE_DIRS}
   ${ELEMENTARY_INCLUDE_DIRS}
   ${MSGPACK_INCLUDE_DIRS}
)
target_include_directories(envim
   PRIVATE
   ${SRC_DIR}/include
)
target_link_libraries(envim
   ${EINA_LIBRARIES}
   ${ECORE_LIBRARIES}
   ${ELEMENTARY_LIBRARIES}
   ${MSGPACK_LIBRARIES}
)

set_property(
   TARGET envim
   PROPERTY C_STANDARD 11
)
if (CMAKE_COMPILER_IS_GNUCC)
   target_compile_options(envim
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
   )
endif ()
