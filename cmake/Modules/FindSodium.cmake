# - Find Sodium
# Find the native libsodium includes and library.
# Once done this will define
#
#  Sodium_INCLUDE_DIR    - where to find libsodium header files, etc.
#  Sodium_LIBRARY        - List of libraries when using libsodium.
#  SODIUM_FOUND          - True if libsodium found.
#

find_library(Sodium_LIBRARY NAMES libsodium.a libsodium.lib sodium HINTS ${Sodium_ROOT_DIR}/lib)
find_path(Sodium_INCLUDE_DIR NAMES sodium.h HINTS ${Sodium_ROOT_DIR}/include)

# handle the QUIETLY and REQUIRED arguments and set SODIUM_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium REQUIRED_VARS Sodium_LIBRARY Sodium_INCLUDE_DIR)

mark_as_advanced(Sodium_LIBRARY Sodium_INCLUDE_DIR)
