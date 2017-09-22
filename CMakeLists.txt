cmake_minimum_required(VERSION 3.0.2)
project(eovim LANGUAGES C VERSION 0.0.99)

include(CPack)

set(CMAKE_MODULE_PATH
   "${CMAKE_MODULE_PATH}${CMAKE_SOURCE_DIR}/cmake/Modules")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THEMES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/themes")
set(BUILD_THEMES_DIR "${CMAKE_BINARY_DIR}/themes")

file(MAKE_DIRECTORY ${BUILD_THEMES_DIR})

find_package(Eina REQUIRED)
find_package(Eet REQUIRED)
find_package(Evas REQUIRED)
find_package(Ecore REQUIRED COMPONENTS File)
find_package(Edje REQUIRED)
find_package(Efreet REQUIRED)
find_package(Elementary REQUIRED)
find_package(MsgPack REQUIRED)

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
   -sd "${THEMES_DIR}/snd"

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
   "${SRC_DIR}/contrib.c"
)
set_source_files_properties(
   "${SRC_DIR}/contrib.c"
   PROPERTIES COMPILE_FLAGS "-w -Wall" # -Wall only
)
target_include_directories(eovim
   SYSTEM PRIVATE
   ${EINA_INCLUDE_DIRS}
   ${EET_INCLUDE_DIRS}
   ${EVAS_INCLUDE_DIRS}
   ${EDJE_INCLUDE_DIRS}
   ${ECORE_INCLUDE_DIRS}
   ${ECORE_FILE_INCLUDE_DIRS}
   ${EFREET_INCLUDE_DIRS}
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
   ${ECORE_FILE_LIBRARIES}
   ${EFREET_LIBRARIES}
   ${ELEMENTARY_LIBRARIES}
   ${MSGPACK_LIBRARIES}
)
add_dependencies(eovim themes)

set_property(
   TARGET eovim
   PROPERTY C_STANDARD 11
)
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
   target_compile_options(eovim
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

install(
   TARGETS eovim
   RUNTIME DESTINATION bin
)
install(
   FILES "${BUILD_THEMES_DIR}/default.edj"
   DESTINATION "share/${CMAKE_PROJECT_NAME}/themes"
)
