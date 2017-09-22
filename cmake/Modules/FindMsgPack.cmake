find_package(PkgConfig REQUIRED)

pkg_search_module(MSGPACK QUIET msgpack)

set(MSGPACK_DEFINITIONS ${MSGPACK_CFLAGS_OTHER})

if (NOT MSGPACK_FOUND)
  find_library(MSGPACK_LIBRARIES NAMES msgpack)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MsgPack
   REQUIRED_VARS MSGPACK_LIBRARIES
)
