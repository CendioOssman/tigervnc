find_path(PIXMAN_INCLUDE_DIR NAMES pixman.h PATH_SUFFIXES pixman-1)

find_library(PIXMAN_LIBRARY NAMES pixman-1)

find_package_handle_standard_args(pixman-1 DEFAULT_MSG PIXMAN_LIBRARY PIXMAN_INCLUDE_DIR)

if(PIXMAN-1_FOUND)
    set(PIXMAN_LIBRARIES ${PIXMAN_LIBRARY})
    set(PIXMAN_INCLUDE_DIRS ${PIXMAN_INCLUDE_DIR})
endif()

if(Pixman_FIND_REQUIRED AND NOT PIXMAN_FOUND)
	message(FATAL_ERROR "Could not find Pixman")
endif()
