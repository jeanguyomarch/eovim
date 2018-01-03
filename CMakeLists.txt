cmake_minimum_required(VERSION 3.0.2)

# Eovim's version works in two different modes:
# (1) major.minor.patch (semver) for stable (released) sources
# (2) major.minor.patch.99 for development (nightly) sources
#
# When a tweak revision (i.e. 99) is added to normal semver (1), Eovim
# considers it is in a development stage. It will automatically add the (short)
# git hash of Eovim to its version, if the sources are from a git repository.
project(eovim LANGUAGES C VERSION 0.1.2.99)

include(CheckTypeSize)

# Try to determine the architecture (32/64 bits) we are compiling for.  This
# will be used to determine in which lib directory plugins must be loaded
# (lib/lib32/lib64).
check_type_size("void*" SIZEOF_VOID_P BUILTIN_TYPES_ONLY LANGUAGE C)
if (SIZEOF_VOID_P EQUAL 4)
   set(LIB_SUFFIX "32")
elseif (SIZEOF_VOID_P EQUAL 8)
   set(LIB_SUFFIX "64")
else ()
   set(LIB_SUFFIX "")
endif ()

# Shared objects (loadable modules) have different extensions from one platform
# to another.  OSX and Linux use .so, whereas Windows uses .dll.  There may be
# more, that I'm not aware of.
if (WIN32)
   set(MODULE_EXT ".dll")
else ()
   set(MODULE_EXT ".so")
endif ()

option(WITH_TESTS "Compile eovim for testing purposes" OFF)
option(WITH_PLUGINS "Compile eovim with plug-ins support" ON)

set(CMAKE_MODULE_PATH
   "${CMAKE_MODULE_PATH}${CMAKE_SOURCE_DIR}/cmake/Modules")

if (WITH_PLUGINS)
   include(plugin)
endif ()
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

execute_process(COMMAND git tag -l WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCHALL "[0-9.]+" VERSION_LIST "${VERSION_STRING}")
list(GET VERSION_LIST -1 VERSION)
set(CPACK_RPM_PACKAGE_RELOCATABLE TRUE)
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "eovim")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Eovim is the Enlightened Neovim")
set(CPACK_PACKAGE_VENDOR "Jean Guyomarc'h")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Eovim is the Enlightened Neovim. That's just an EFL GUI client for Neovim.")
set(CPACK_RPM_PACKAGE_REQUIRES "neovim >= 0.2.0, efl >= 1.20")

include(CPack)

find_package(Eina REQUIRED)
find_package(Eet REQUIRED)
find_package(Evas REQUIRED)
find_package(Ecore REQUIRED COMPONENTS File Input)
find_package(Edje REQUIRED)
find_package(Efreet REQUIRED)
find_package(Elementary REQUIRED)
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
   "${SRC_DIR}/config.c"
   "${SRC_DIR}/mode.c"
   "${SRC_DIR}/keymap.c"
   "${SRC_DIR}/gui.c"
   "${SRC_DIR}/prefs.c"
   "${SRC_DIR}/termview.c"
   "${SRC_DIR}/nvim_event.c"
   "${SRC_DIR}/nvim_api.c"
   "${SRC_DIR}/nvim_helper.c"
   "${SRC_DIR}/plugin.c"
   "${SRC_DIR}/options.c"
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
   ${ECORE_INPUT_INCLUDE_DIRS}
   ${EFREET_INCLUDE_DIRS}
   ${ELEMENTARY_INCLUDE_DIRS}
   ${MSGPACK_INCLUDE_DIRS}
)
target_include_directories(eovim
   PRIVATE
   "${SRC_DIR}/include"
   "${BUILD_INCLUDE_DIR}"
)
target_link_libraries(eovim
   ${EINA_LIBRARIES}
   ${EET_LIBRARIES}
   ${EVAS_LIBRARIES}
   ${EDJE_LIBRARIES}
   ${ECORE_LIBRARIES}
   ${ECORE_FILE_LIBRARIES}
   ${ECORE_INPUT_LIBRARIES}
   ${EFREET_LIBRARIES}
   ${ELEMENTARY_LIBRARIES}
   ${MSGPACK_LIBRARIES}
)
add_dependencies(eovim themes)
add_nazi_compiler_warnings(eovim)
target_compile_definitions(eovim
   PRIVATE
   PACKAGE_BIN_DIR=\"${CMAKE_INSTALL_PREFIX}/bin\"
   PACKAGE_LIB_DIR=\"${CMAKE_INSTALL_PREFIX}/lib\"
   PACKAGE_DATA_DIR=\"${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}\"
   SOURCE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\"
   BUILD_DATA_DIR=\"${CMAKE_BINARY_DIR}\"
   BUILD_PLUGINS_DIR=\"${CMAKE_BINARY_DIR}/plugins\"
   LIB_SUFFIX=\"${LIB_SUFFIX}\"
   MODULE_EXT=\"${MODULE_EXT}\"
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
# Plug-ins support
##############################################################################
if (WITH_PLUGINS)
   # Tell Eovim that we use plugins
   target_compile_definitions(eovim PRIVATE HAVE_PLUGINS=1)

   # Install Eovim's headers: Eovim.h and the headers it includes.
   install(FILES
      "${CMAKE_SOURCE_DIR}/src/include/Eovim.h"
      DESTINATION include/eovim-0)
   install(FILES
      "${CMAKE_SOURCE_DIR}/src/include/eovim/types.h"
      "${CMAKE_SOURCE_DIR}/src/include/eovim/msgpack_helper.h"
      "${BUILD_INCLUDE_DIR}/eovim/version.h"
      DESTINATION include/eovim-0/eovim)
   add_subdirectory(plugins)

   # When plugins are enabled, eovim must be linked against its plugins runtime
   # library: libeovim.
   target_link_libraries(eovim libeovim)
endif ()

if (WITH_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif ()


##############################################################################
# Man page
##############################################################################
add_subdirectory(data/man)
