cmake_minimum_required(VERSION 3.0.2)
project(eovim LANGUAGES C VERSION 0.0.99)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THEMES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/themes")
set(BUILD_THEMES_DIR "${CMAKE_BINARY_DIR}/themes")

file(MAKE_DIRECTORY ${BUILD_THEMES_DIR})

find_package(PkgConfig REQUIRED)
find_package(Eina REQUIRED)
find_package(Eet REQUIRED)
find_package(Evas REQUIRED)
find_package(Ecore REQUIRED)
find_package(Edje REQUIRED)
find_package(Elementary REQUIRED)
pkg_check_modules(MSGPACK msgpack REQUIRED)

add_custom_command(
   OUTPUT "${BUILD_THEMES_DIR}/default.edj"

   DEPENDS
   "${THEMES_DIR}/default.edc"

   VERBATIM
   COMMAND
   "${EDJE_CC_EXECUTABLE}"
   "${THEMES_DIR}/default.edc"
   "${BUILD_THEMES_DIR}/default.edj"
   -author "${CMAKE_SOURCE_DIR}/AUTHORS"
   -license "${CMAKE_SOURCE_DIR}/LICENSE"
   -id "${THEMES_DIR}/img"

   COMMENT "Generating Edje Theme"
)
add_custom_target(themes
   DEPENDS "${BUILD_THEMES_DIR}/default.edj"
)

add_executable(eovim
   "${SRC_DIR}/main.c"
   "${SRC_DIR}/nvim.c"
   "${SRC_DIR}/config.c"
   "${SRC_DIR}/mode.c"
   "${SRC_DIR}/keymap.c"
   "${SRC_DIR}/gui.c"
   "${SRC_DIR}/termview.c"
   "${SRC_DIR}/nvim_event.c"
   "${SRC_DIR}/nvim_api.c"
)
target_include_directories(eovim
   SYSTEM PRIVATE
   ${EINA_INCLUDE_DIRS}
   ${EET_INCLUDE_DIRS}
   ${EVAS_INCLUDE_DIRS}
   ${EDJE_INCLUDE_DIRS}
   ${ECORE_INCLUDE_DIRS}
   ${ELEMENTARY_INCLUDE_DIRS}
   ${MSGPACK_INCLUDE_DIRS}
)
target_include_directories(eovim
   PRIVATE
   "${SRC_DIR}/include"
)
target_link_libraries(eovim
   ${EINA_LIBRARIES}
   ${EET_LIBRARIES}
   ${EVAS_LIBRARIES}
   ${EDJE_LIBRARIES}
   ${ECORE_LIBRARIES}
   ${ELEMENTARY_LIBRARIES}
   ${MSGPACK_LIBRARIES}
)
add_dependencies(eovim themes)

set_property(
   TARGET eovim
   PROPERTY C_STANDARD 11
)
if (CMAKE_COMPILER_IS_GNUCC)
   target_compile_options(eovim
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
target_compile_definitions(eovim
   PRIVATE
   PACKAGE_BIN_DIR=\"${CMAKE_INSTALL_PREFIX}/bin\"
   PACKAGE_LIB_DIR=\"${CMAKE_INSTALL_PREFIX}/lib\"
   PACKAGE_DATA_DIR=\"${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}\"
   BUILD_DATA_DIR=\"${CMAKE_BINARY_DIR}\"
)
