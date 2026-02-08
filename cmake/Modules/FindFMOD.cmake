if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Android FMOD configuration
    set(FMOD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/android/include")
    set(FMOD_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/android/lib/${ANDROID_ABI}")

    add_library(FMOD::fmod SHARED IMPORTED)
    set_target_properties(FMOD::fmod PROPERTIES
        IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/libfmod.so"
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
    )

    add_library(FMOD::fmodL SHARED IMPORTED)
    set_target_properties(FMOD::fmodL PROPERTIES
        IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/libfmodL.so"
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
    )

elseif(WIN32)
    # Windows FMOD configuration
    set(FMOD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/windows/include")
    set(FMOD_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/windows/lib/x64")

    add_library(FMOD::fmod SHARED IMPORTED)
    set_target_properties(FMOD::fmod PROPERTIES
            # For Windows, IMPORTED_LOCATION is the .dll,
            # and IMPORTED_IMPLIB is the .lib or .dll.a
            IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/fmod.dll"
            IMPORTED_IMPLIB "${FMOD_LIBRARY_DIR}/libfmod.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
        )

    add_library(FMOD::fmodL SHARED IMPORTED)
    set_target_properties(FMOD::fmodL PROPERTIES
            IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/fmodL.dll"
            IMPORTED_IMPLIB "${FMOD_LIBRARY_DIR}/libfmodL.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux FMOD configuration
    set(FMOD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/linux/include")

    # Determine architecture directory
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(FMOD_LINUX_ARCH_DIR "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(FMOD_LINUX_ARCH_DIR "x86_64")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture for FMOD: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(FMOD_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/linux/lib/${FMOD_LINUX_ARCH_DIR}")

    add_library(FMOD::fmod SHARED IMPORTED)
    set_target_properties(FMOD::fmod PROPERTIES
        IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/libfmod.so"
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
    )

    add_library(FMOD::fmodL SHARED IMPORTED)
    set_target_properties(FMOD::fmodL PROPERTIES
        IMPORTED_LOCATION "${FMOD_LIBRARY_DIR}/libfmodL.so"
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
    )
else()
    message(FATAL_ERROR "FMOD is not configured for the current system: ${CMAKE_SYSTEM_NAME}")
endif()

# Set FMOD_FOUND and FMOD_LIBRARIES for compatibility
set(FMOD_FOUND TRUE)
set(FMOD_LIBRARIES FMOD::fmod)
set(FMOD_LIBRARIES_DEBUG FMOD::fmodL)
