#
# find libuuid
#
find_path(UUID_INCLUDE_DIR
  NAMES
    uuid/uuid.h
  PATHS
    $ENV{UUID_HOME}/include
    /usr/include
    /usr/local/include
)

find_library(UUID_LIBRARIES
  NAMES
    uuid
  PATHS
    $ENV{UUID_HOME}/lib
    /usr/lib
    /usr/lib64
    /usr/local/lib
    /usr/local/lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  uuid
  DEFAULT_MSG
  UUID_LIBRARIES
  UUID_INCLUDE_DIR
)

mark_as_advanced(
  UUID_INCLUDE_DIR
  UUID_LIBRARIES
)
