# ──────────────────────────────────────────────────────────────
#  cmake/copy_dlls.cmake
#  Post-build script (Windows only): collects runtime DLLs
#  needed by the executable and places them next to it.
#
#  Invoked from CMakeLists.txt:
#    cmake -D _EXE=<path-to-exe>
#          -D _OUTPUT_DIR=<output-directory>
#          -P cmake/copy_dlls.cmake
# ──────────────────────────────────────────────────────────────

if(NOT _EXE)
    message(FATAL_ERROR "_EXE not set")
endif()

if(NOT _OUTPUT_DIR)
    message(FATAL_ERROR "_OUTPUT_DIR not set")
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_EXE}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    POST_EXCLUDE_REGEXES ""
)

foreach(_lib ${_resolved})
    get_filename_component(_lib_name "${_lib}" NAME)
    set(_dst "${_OUTPUT_DIR}/${_lib_name}")
    if(NOT EXISTS "${_dst}")
        file(COPY_FILE "${_lib}" "${_dst}")
        message(STATUS "  Copied DLL: ${_lib_name}")
    endif()
endforeach()

if(_unresolved)
    list(REMOVE_DUPLICATES _unresolved)
    message(WARNING "Unresolved runtime dependencies: ${_unresolved}")
endif()
