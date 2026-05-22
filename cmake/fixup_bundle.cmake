# ──────────────────────────────────────────────────────────────
#  cmake/fixup_bundle.cmake  (macOS)
#  Post-build script: copies ALL runtime .dylib dependencies
#  (recursive / transitive, including @rpath-resolved) into the
#  .app bundle's Frameworks directory and fixes up install names.
#
#  Invocation (in CMakeLists.txt POST_BUILD):
#    cmake -D _EXE=<path-to-exe>
#          -D _BUNDLE=<path-to-bundle-dir>
#          -P cmake/fixup_bundle.cmake
# ──────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.21)

if(NOT _EXE)
    message(FATAL_ERROR "_EXE not set")
endif()
if(NOT _BUNDLE)
    message(FATAL_ERROR "_BUNDLE not set")
endif()

set(_FRAMEWORKS_DIR "${_BUNDLE}/Contents/Frameworks")

if(NOT EXISTS "${_EXE}")
    message(FATAL_ERROR "Executable not found: ${_EXE}")
endif()

message(STATUS "=== Fixing up macOS bundle ===")
message(STATUS "  Executable: ${_EXE}")

# ── 1. Use CMake's GET_RUNTIME_DEPENDENCIES to resolve the full tree ──
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_EXE}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
)

if(_unresolved)
    list(REMOVE_DUPLICATES _unresolved)
    message(WARNING "Unresolved dependencies (may be optional): ${_unresolved}")
endif()

# Filter: keep only non-system third-party libraries
set(_to_bundle)
set(_system_prefixes
    "/usr/lib/"
    "/System/"
    "/usr/lib/swift/"
)
foreach(_lib ${_resolved})
    set(_is_system FALSE)
    foreach(_prefix ${_system_prefixes})
        if(_lib MATCHES "^${_prefix}")
            set(_is_system TRUE)
            break()
        endif()
    endforeach()
    if(NOT _is_system AND NOT _lib MATCHES "libSystem\\.B")
        list(APPEND _to_bundle "${_lib}")
    endif()
endforeach()

if(NOT _to_bundle)
    message(STATUS "  No non-system dylib dependencies to bundle.")
    return()
endif()

list(REMOVE_DUPLICATES _to_bundle)
message(STATUS "  Libraries to bundle: ${_to_bundle}")

# ── 2. Create Frameworks/ directory ──
file(MAKE_DIRECTORY "${_FRAMEWORKS_DIR}")

# ── 3. Copy ALL collected dylibs into Frameworks/ ──
set(_bundled_names)
foreach(_lib ${_to_bundle})
    if(NOT EXISTS "${_lib}")
        message(WARNING "  Library not found, skipping: ${_lib}")
        continue()
    endif()
    get_filename_component(_lib_name "${_lib}" NAME)
    set(_dst "${_FRAMEWORKS_DIR}/${_lib_name}")
    if(NOT EXISTS "${_dst}")
        file(COPY_FILE "${_lib}" "${_dst}")
        message(STATUS "  Copied: ${_lib_name}")
    endif()
    list(APPEND _bundled_names "${_lib_name}")
endforeach()

list(REMOVE_DUPLICATES _bundled_names)

# ── 4. Fix EVERY file in Frameworks/ (id + internal refs) ──
file(GLOB _framework_files "${_FRAMEWORKS_DIR}/*.dylib")
foreach(_dst IN LISTS _framework_files)
    get_filename_component(_lib_name "${_dst}" NAME)

    # Fix own id
    execute_process(
        COMMAND install_name_tool -id "@rpath/${_lib_name}" "${_dst}"
        OUTPUT_QUIET ERROR_QUIET
    )

    # Fix references to other libraries — parse otool -L and match by filename
    execute_process(
        COMMAND otool -L "${_dst}"
        OUTPUT_VARIABLE _dep_otool
        ERROR_QUIET
    )
    string(REPLACE "\n" ";" _dep_lines "${_dep_otool}")
    foreach(_dep IN LISTS _dep_lines)
        string(STRIP "${_dep}" _dep)
        if(_dep MATCHES "^/" OR _dep MATCHES "^@rpath/")
            # Extract path before first space
            string(REGEX REPLACE "^(/[^ ]+).*" "\\1" _dep_path "${_dep}")
            if(NOT _dep_path)
                string(REGEX REPLACE "^@rpath/([^ ]+).*" "\\1" _dep_name "${_dep}")
            else()
                get_filename_component(_dep_name "${_dep_path}" NAME)
            endif()
            if(_dep_name AND _dep_name IN_LIST _bundled_names)
                execute_process(
                    COMMAND install_name_tool -change "${_dep_path}" "@rpath/${_dep_name}" "${_dst}"
                    OUTPUT_QUIET ERROR_QUIET
                )
                message(STATUS "    ${_lib_name}: ${_dep_name} → @rpath/${_dep_name}")
            endif()
        endif()
    endforeach()
endforeach()

# ── 5. Fix the main executable's library references ──
execute_process(
    COMMAND otool -L "${_EXE}"
    OUTPUT_VARIABLE _exe_otool
    ERROR_QUIET
)
string(REPLACE "\n" ";" _exe_lines "${_exe_otool}")
foreach(_line IN LISTS _exe_lines)
    string(STRIP "${_line}" _line)
    if(_line MATCHES "^/" OR _line MATCHES "^@rpath/")
        string(REGEX REPLACE "^(/[^ ]+).*" "\\1" _lib_path "${_line}")
        if(NOT _lib_path)
            string(REGEX REPLACE "^@rpath/([^ ]+).*" "\\1" _lib_name "${_line}")
        else()
            get_filename_component(_lib_name "${_lib_path}" NAME)
        endif()
        if(_lib_name AND _lib_name IN_LIST _bundled_names)
            execute_process(
                COMMAND install_name_tool -change "${_lib_path}" "@rpath/${_lib_name}" "${_EXE}"
                OUTPUT_QUIET ERROR_QUIET
            )
            message(STATUS "  Exe: ${_lib_name} → @rpath/${_lib_name}")
        endif()
    endif()
endforeach()

# ── 6. Clean up rpaths and add Frameworks/ rpath ──
execute_process(
    COMMAND otool -l "${_EXE}"
    COMMAND grep -A2 "LC_RPATH"
    COMMAND grep "path "
    COMMAND sed "s/.*path //g"
    COMMAND sed "s/ (offset.*//g"
    OUTPUT_VARIABLE _rpaths_str
    ERROR_QUIET
)
string(REPLACE "\n" ";" _rpaths "${_rpaths_str}")
foreach(_rp IN LISTS _rpaths)
    string(STRIP "${_rp}" _rp)
    if(_rp AND NOT _rp STREQUAL "@executable_path/../Frameworks")
        execute_process(
            COMMAND install_name_tool -delete_rpath "${_rp}" "${_EXE}"
            OUTPUT_QUIET ERROR_QUIET
        )
        message(STATUS "  Removed rpath: ${_rp}")
    endif()
endforeach()

execute_process(
    COMMAND install_name_tool -add_rpath "@executable_path/../Frameworks" "${_EXE}"
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE _rp_add_result
)
if(_rp_add_result EQUAL 0)
    message(STATUS "  Added rpath: @executable_path/../Frameworks")
endif()

# ── 7. Re-sign everything we modified (install_name_tool invalidates signatures) ──
message(STATUS "  Re-signing modified binaries...")
file(GLOB _framework_files "${_FRAMEWORKS_DIR}/*.dylib")
foreach(_dst IN LISTS _framework_files)
    execute_process(
        COMMAND codesign --force --sign - "${_dst}"
        OUTPUT_QUIET ERROR_QUIET
    )
endforeach()
execute_process(
    COMMAND codesign --force --sign - "${_EXE}"
    OUTPUT_QUIET ERROR_QUIET
)
execute_process(
    COMMAND codesign --force --deep --sign - "${_BUNDLE}"
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE _cs_result
)
if(_cs_result EQUAL 0)
    message(STATUS "  Re-signed bundle successfully")
else()
    message(WARNING "  Bundle code signing returned ${_cs_result}")
endif()

message(STATUS "=== Bundle fixup complete! ===")
