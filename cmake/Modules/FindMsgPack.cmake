find_package(PkgConfig REQUIRED)
pkg_search_module(MSGPACK QUIET msgpack)

# No pkg-config was shipped? Hope for the best by trying to find where the
# damn thing is located. That's our last resort.
if (NOT MSGPACK_FOUND)
   find_library(MSGPACK_LIBRARIES NAMES msgpack msgpackc)
endif ()

# Validate the package with MSGPACK_LIBRARIES find above
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
   MsgPack REQUIRED_VARS MSGPACK_LIBRARIES)
