# We first start to search for msgpack in the .deps/ directoy.  If we ran the
# get-msgpack.sh script before, we would find there a hand-compiled msgpack
find_library(MSGPACK_LIBRARIES msgpackc
   PATHS "${CMAKE_SOURCE_DIR}/.deps/_sysroot/lib"
   NO_DEFAULT_PATH)

if (MSGPACK_LIBRARIES)
   # If we found the built-in msgpack, set the include directories
   # as well.
   set(MSGPACK_INCLUDE_DIRS
      "${CMAKE_SOURCE_DIR}/.deps/_sysroot/include")
else ()
   # No built-in? Try a system one with pkg-config.
   find_package(PkgConfig REQUIRED)
   pkg_search_module(MSGPACK QUIET msgpack)

   # No pkg-config was shipped? Hope for the best by trying to find where the
   # damn thing is located. That's our last resort.
   if (NOT MSGPACK_FOUND)
      find_library(MSGPACK_LIBRARIES NAMES msgpack msgpackc)
   endif ()

endif ()

# Validate the package with MSGPACK_LIBRARIES find above
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
   MsgPack REQUIRED_VARS MSGPACK_LIBRARIES)
