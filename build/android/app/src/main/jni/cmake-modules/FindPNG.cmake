# Drop-in FindPNG.cmake that wires consumers (libgd, …) up to whichever
# in-tree libpng was added first via add_subdirectory(): either ours
# under build/android/deps/libpng, or the copy SDL_image vendors at
# deps/SDL_image/external/libpng. Both define a `png_static` target.

if(TARGET png_static)
    set(PNG_FOUND TRUE)
    # Pull the include dirs straight off the target so we work
    # regardless of where in the source tree the libpng copy lives.
    get_target_property(_png_inc png_static INCLUDE_DIRECTORIES)
    if(NOT _png_inc)
        get_target_property(_png_inc png_static INTERFACE_INCLUDE_DIRECTORIES)
    endif()
    set(PNG_INCLUDE_DIR ${_png_inc})
    set(PNG_INCLUDE_DIRS ${_png_inc})
    set(PNG_LIBRARY png_static)
    set(PNG_LIBRARIES png_static)
    set(PNG_VERSION_STRING "1.6.43")
    if(NOT TARGET PNG::PNG)
        add_library(PNG::PNG ALIAS png_static)
    endif()
else()
    include("${CMAKE_ROOT}/Modules/FindPNG.cmake")
endif()
