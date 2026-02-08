# FindBreakpad.cmake - Finds the Google Breakpad client library
#
# This module locates the Breakpad headers and the client static library.
#
# This module will define the following variables:
#   BREAKPAD_FOUND          - True if the Breakpad library and headers were found.
#   BREAKPAD_INCLUDE_DIRS   - The location of the Breakpad header files.
#   BREAKPAD_LIBRARIES      - The location of the client static library.
#
# It will also create the following IMPORTED target:
#   Breakpad::breakpad_client - The breakpad client static library.
#
# You can provide a hint to where Breakpad is installed by setting BREAKPAD_ROOT,
# either as an environment variable or as a CMake variable.
#   e.g., cmake -DBREAKPAD_ROOT=/path/to/breakpad .

# Step 1: Find the Breakpad include directory by looking for a key header.
# Breakpad headers are usually within a 'src' directory.
find_path(BREAKPAD_INCLUDE_DIR
    NAMES client/linux/handler/exception_handler.h # A representative header file
    HINTS
        ${BREAKPAD_ROOT}
    PATH_SUFFIXES
        src # Headers are almost always in the 'src' dir
        include
)

# Step 2: Find the Breakpad client static library
find_library(BREAKPAD_LIBRARY
    NAMES breakpad_client # The library is usually called libbreakpad_client.a
    HINTS
        ${BREAKPAD_ROOT}
    PATH_SUFFIXES
        # Common installation paths, often in a platform-specific subfolder
        src/client/linux
        lib
)

# Step 3: Use the standard CMake module to handle the FOUND variable and error messages
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Breakpad
    FOUND_VAR BREAKPAD_FOUND
    REQUIRED_VARS BREAKPAD_LIBRARY BREAKPAD_INCLUDE_DIR
)

# Step 4: If found, create the imported target
if(BREAKPAD_FOUND)
    # Set public variables for convenience
    set(BREAKPAD_INCLUDE_DIRS ${BREAKPAD_INCLUDE_DIR})
    set(BREAKPAD_LIBRARIES ${BREAKPAD_LIBRARY})

    # Create the imported target
    if(NOT TARGET Breakpad::breakpad_client)
        add_library(Breakpad::breakpad_client STATIC IMPORTED)
        set_target_properties(Breakpad::breakpad_client PROPERTIES
            IMPORTED_LOCATION "${BREAKPAD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BREAKPAD_INCLUDE_DIR}"
        )
    endif()
endif()

# Mark these variables as advanced so they don't clutter the CMake GUI
mark_as_advanced(BREAKPAD_INCLUDE_DIR BREAKPAD_LIBRARY)
