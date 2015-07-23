# - Find spdlog
# Find spdlog library.
# Once done this will define
#
#  Spdlog_INCLUDE_DIR    - where to find spdlog header files, etc.
#  SPDLOG_FOUND          - True if spdlog found.
#

find_path(Spdlog_INCLUDE_DIR NAMES spdlog/spdlog.h HINTS ${Spdlog_ROOT_DIR}/include ${CMAKE_SOURCE_DIR}/external/spdlog/include)

# handle the QUIETLY and REQUIRED arguments and set SPDLOG_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Spdlog REQUIRED_VARS Spdlog_INCLUDE_DIR)

mark_as_advanced(Spdlog_INCLUDE_DIR)
