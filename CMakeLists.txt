cmake_minimum_required(VERSION 3.0.2)

# Eovim's version works in two different modes:
# (1) major.minor.patch (semver) for stable (released) sources
# (2) major.minor.patch.99 for development (nightly) sources
#
# When a tweak revision (i.e. 99) is added to normal semver (1), Eovim
# considers it is in a development stage. It will automatically add the (short)
# git hash of Eovim to its version, if the sources are from a git repository.
project(eovim LANGUAGES C VERSION 0.1.3.99)

include(CheckTypeSize)

set(CMAKE_MODULE_PATH
   "${CMAKE_MODULE_PATH}${CMAKE_SOURCE_DIR}/cmake/Modules")

include(compiler_warnings)
include(git_commit)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THEMES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/themes")
set(BUILD_THEMES_DIR "${CMAKE_BINARY_DIR}/themes")
set(BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")

file(MAKE_DIRECTORY "${BUILD_THEMES_DIR}")
file(MAKE_DIRECTORY "${BUILD_INCLUDE_DIR}/eovim")
configure_file(
   "${CMAKE_SOURCE_DIR}/cmake/Modules/version.h.in"
   "${BUILD_INCLUDE_DIR}/eovim/version.h"
   @ONLY)

# CPack configuration
include(cpack_config)

find_package(Efl 1.19 REQUIRED COMPONENTS
  eina eet edje ecore-file ecore-input edje evas efreet elementary)
find_program(EDJE_CC_EXECUTABLE edje_cc)
if (NOT EDJE_CC_EXECUTABLE)
  message(FATAL_ERROR "Failed to find edje_cc program")
endif ()
find_package(MsgPack REQUIRED)

add_custom_command(
   OUTPUT "${BUILD_THEMES_DIR}/default.edj"

   DEPENDS
   "${THEMES_DIR}/default.edc"
   "${THEMES_DIR}/cursor.edc"
   "${THEMES_DIR}/cmdline.edc"
   "${THEMES_DIR}/completion.edc"
   "${THEMES_DIR}/tab.edc"

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
   "${SRC_DIR}/keymap.c"
   "${SRC_DIR}/gui.c"
   "${SRC_DIR}/termview.c"
   "${SRC_DIR}/nvim_event.c"
   "${SRC_DIR}/event/option_set.c"
   "${SRC_DIR}/event/mode.c"
   "${SRC_DIR}/event/cmdline.c"
   "${SRC_DIR}/event/eovim.c"
   "${SRC_DIR}/nvim_api.c"
   "${SRC_DIR}/nvim_attach.c"
   "${SRC_DIR}/nvim_completion.c"
   "${SRC_DIR}/nvim_helper.c"
   "${SRC_DIR}/nvim_request.c"
   "${SRC_DIR}/options.c"
   "${SRC_DIR}/contrib.c"
)
set_source_files_properties(
   "${SRC_DIR}/contrib.c"
   PROPERTIES COMPILE_FLAGS "-w -Wall" # -Wall only
)
target_include_directories(eovim
   SYSTEM PRIVATE
   ${EFL_INCLUDE_DIRS}
   ${MSGPACK_INCLUDE_DIRS}
)
target_include_directories(eovim
   PRIVATE
   "${CMAKE_SOURCE_DIR}/include"
   "${BUILD_INCLUDE_DIR}"
)
target_link_libraries(eovim
   ${EFL_LIBRARIES}
   ${MSGPACK_LIBRARIES}
)
add_dependencies(eovim themes)
add_nazi_compiler_warnings(eovim)
target_compile_definitions(eovim
   PRIVATE
   PACKAGE_BIN_DIR=\"${CMAKE_INSTALL_PREFIX}/bin\"
   PACKAGE_LIB_DIR=\"${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}\"
   PACKAGE_DATA_DIR=\"${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}\"
   SOURCE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\"
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
install(FILES
   "${CMAKE_SOURCE_DIR}/data/images/led_dark.png"
   "${CMAKE_SOURCE_DIR}/data/images/led_light.png"
   "${CMAKE_SOURCE_DIR}/data/images/neovim.png"
   DESTINATION "share/${CMAKE_PROJECT_NAME}/images"
)
install(FILES
   "${CMAKE_SOURCE_DIR}/data/vim/runtime.vim"
   DESTINATION "share/${CMAKE_PROJECT_NAME}/vim"
)
install(
   FILES "${CMAKE_SOURCE_DIR}/data/desktop/eovim.desktop"
   DESTINATION "share/applications"
)
install(
   FILES "${CMAKE_SOURCE_DIR}/data/images/eovim.png"
   DESTINATION "share/icons"
)

##############################################################################
# Man page
##############################################################################
add_subdirectory(data/man)
