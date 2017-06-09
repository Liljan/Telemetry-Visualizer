# Check for root directory existence. Can be overwritten with environment variable SR_<LIBRARY NAME>_ROOT.
find_package_root(MONGO_ROOT mongoc)

# Set definitions
set(MONGO_DEFINITIONS)

# Set library directories
set(MONGO_BASE_DIR "${MONGO_ROOT}/lib")

# Set include directories
set(MONGO_INCLUDE_DIRS "${MONGO_ROOT}/include/libbson-1.0" "${MONGO_ROOT}/include/libmongoc-1.0")

# Set libraries
set(MONGO_LIBRARIES
    # TODO: Not sure if /dev/*.lib are for debug or dev and release builds or the opposite?
    "${MONGO_BASE_DIR}/$<$<CONFIG:DEBUG>:>$<$<NOT:$<CONFIG:DEBUG>>:dev>/bson-1.0${LIB_SUFFIX}"
    "${MONGO_BASE_DIR}/$<$<CONFIG:DEBUG>:>$<$<NOT:$<CONFIG:DEBUG>>:dev>/mongoc-1.0${LIB_SUFFIX}"
    "${MONGO_BASE_DIR}/$<$<CONFIG:DEBUG>:>$<$<NOT:$<CONFIG:DEBUG>>:dev>/bson-static-1.0${LIB_SUFFIX}"
    "${MONGO_BASE_DIR}/$<$<CONFIG:DEBUG>:>$<$<NOT:$<CONFIG:DEBUG>>:dev>/mongoc-static-1.0${LIB_SUFFIX}")

# Set binaries
set(MONGO_BINARIES)

# Check if package setup is successful
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MONGO DEFAULT_MSG MONGO_ROOT MONGO_INCLUDE_DIRS MONGO_LIBRARIES)
mark_as_advanced(MONGO_INCLUDE_DIRS MONGO_LIBRARIES)
