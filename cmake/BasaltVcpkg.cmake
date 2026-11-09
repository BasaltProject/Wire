# Configures vcpkg manifest mode, overlay ports/triplets, and target triplet before project().

if(DEFINED BASALT_VCPKG_CONFIGURED)
    return()
endif()
set(BASALT_VCPKG_CONFIGURED TRUE)

set(_basalt_wire_root "${CMAKE_CURRENT_LIST_DIR}/..")
cmake_path(NORMAL_PATH _basalt_wire_root)

set(VCPKG_OVERLAY_PORTS "${_basalt_wire_root}/vcpkg-overlays/ports" CACHE STRING "Basalt vcpkg overlay ports" FORCE)
set(VCPKG_OVERLAY_TRIPLETS "${_basalt_wire_root}/vcpkg-overlays/triplets" CACHE STRING "Basalt vcpkg overlay triplets" FORCE)

# CMAKE_HOST_SYSTEM_PROCESSOR is only populated by project(); fall back to uname before it.
set(_basalt_host_arch "")
if(DEFINED CMAKE_HOST_SYSTEM_PROCESSOR AND NOT "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "")
    set(_basalt_host_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
elseif(NOT "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _basalt_host_arch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET OR "${VCPKG_TARGET_TRIPLET}" STREQUAL "")
    if(ANDROID OR DEFINED ANDROID_ABI OR "${CMAKE_TOOLCHAIN_FILE}" MATCHES "android\\.toolchain\\.cmake")
        if("${ANDROID_ABI}" STREQUAL "x86_64")
            set(_basalt_triplet "x64-android")
        else()
            set(_basalt_triplet "arm64-android")
        endif()
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD" OR "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "FreeBSD")
        if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$" OR (NOT DEFINED CMAKE_SYSTEM_PROCESSOR AND "${_basalt_host_arch}" MATCHES "^(aarch64|arm64)$"))
            set(_basalt_triplet "arm64-freebsd")
        else()
            set(_basalt_triplet "x64-freebsd")
        endif()
    elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin" OR APPLE)
        if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64" OR (NOT DEFINED CMAKE_OSX_ARCHITECTURES AND "${_basalt_host_arch}" STREQUAL "x86_64"))
            set(_basalt_triplet "x64-osx")
        else()
            set(_basalt_triplet "arm64-osx")
        endif()
    elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows" OR WIN32)
        if("${CMAKE_C_COMPILER_TARGET}" MATCHES "^aarch64" OR "${CMAKE_CXX_COMPILER_TARGET}" MATCHES "^aarch64" OR "$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64" OR "$ENV{PROCESSOR_ARCHITEW6432}" STREQUAL "ARM64")
            set(_basalt_triplet "arm64-windows-static-md")
        else()
            set(_basalt_triplet "x64-windows-static-md")
        endif()
    else()
        if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$" OR (NOT DEFINED CMAKE_SYSTEM_PROCESSOR AND "${_basalt_host_arch}" MATCHES "^(aarch64|arm64)$"))
            set(_basalt_arch "arm64")
        else()
            set(_basalt_arch "x64")
        endif()
        if(BASALT_LINUX_MUSL OR EXISTS "/etc/alpine-release")
            set(_basalt_triplet "${_basalt_arch}-linux-musl")
        else()
            set(_basalt_triplet "${_basalt_arch}-linux")
        endif()
    endif()
    set(VCPKG_TARGET_TRIPLET "${_basalt_triplet}" CACHE STRING "vcpkg target triplet")
endif()

# vcpkg downloads glibc-only cmake/ninja tools during install; use the system ones on musl and FreeBSD.
if(BASALT_LINUX_MUSL OR EXISTS "/etc/alpine-release" OR "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "FreeBSD")
    set(ENV{VCPKG_FORCE_SYSTEM_BINARIES} "1")
endif()

if(NOT "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    if(DEFINED CMAKE_C_COMPILER AND NOT "$ENV{CC}" STREQUAL "")
        # Keep existing CC
    elseif(DEFINED CMAKE_C_COMPILER)
        set(ENV{CC} "${CMAKE_C_COMPILER}")
    endif()
    if(DEFINED CMAKE_CXX_COMPILER AND NOT "$ENV{CXX}" STREQUAL "")
        # Keep existing CXX
    elseif(DEFINED CMAKE_CXX_COMPILER)
        set(ENV{CXX} "${CMAKE_CXX_COMPILER}")
    endif()
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD" AND NOT "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "FreeBSD")
    set(_basalt_chainload "${CMAKE_BINARY_DIR}/basalt-freebsd-chainload.cmake")
    set(_chainload_content "set(CMAKE_SYSTEM_NAME FreeBSD)\n")
    foreach(_var CMAKE_SYSTEM_PROCESSOR CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_ASM_COMPILER
                 CMAKE_C_COMPILER_TARGET CMAKE_CXX_COMPILER_TARGET CMAKE_ASM_COMPILER_TARGET
                 CMAKE_SYSROOT CMAKE_CXX_STANDARD_LIBRARIES
                 CMAKE_FIND_ROOT_PATH_MODE_PROGRAM CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
                 CMAKE_FIND_ROOT_PATH_MODE_INCLUDE CMAKE_FIND_ROOT_PATH_MODE_PACKAGE)
        if(DEFINED ${_var} AND NOT "${${_var}}" STREQUAL "")
            string(APPEND _chainload_content "set(${_var} \"${${_var}}\" CACHE STRING \"\" FORCE)\n")
        endif()
    endforeach()
    if(DEFINED CMAKE_CXX_FLAGS)
        string(APPEND _chainload_content "string(APPEND CMAKE_CXX_FLAGS_INIT \" -fPIC ${CMAKE_CXX_FLAGS} \")\n")
    endif()
    string(APPEND _chainload_content "string(APPEND CMAKE_C_FLAGS_INIT \" -fPIC \")\n")
    if(DEFINED CMAKE_EXE_LINKER_FLAGS)
        string(APPEND _chainload_content "string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT \" ${CMAKE_EXE_LINKER_FLAGS} \")\n")
        string(APPEND _chainload_content "string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT \" ${CMAKE_EXE_LINKER_FLAGS} \")\n")
    endif()
    string(APPEND _chainload_content "list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_CXX_STANDARD_LIBRARIES)\n")
    file(WRITE "${_basalt_chainload}" "${_chainload_content}")
    set(ENV{BASALT_VCPKG_CHAINLOAD_TOOLCHAIN} "${_basalt_chainload}")
    if(NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_basalt_chainload}" CACHE FILEPATH "")
    endif()
endif()

# Read the pinned builtin-baseline so the checkout can be aligned with it.
set(_basalt_baseline "")
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
    file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" _manifest_json)
    string(JSON _basalt_baseline ERROR_VARIABLE _json_err GET "${_manifest_json}" "builtin-baseline")
    if(_json_err)
        set(_basalt_baseline "")
    endif()
endif()
if(_basalt_baseline)
    set(_basalt_baseline_ref "${_basalt_baseline}")
else()
    set(_basalt_baseline_ref "master")
endif()

# Fetch a shallow vcpkg snapshot into dir and detach HEAD at ref.
# Sets _basalt_snapshot_ok and _basalt_snapshot_changed.
macro(basalt_vcpkg_snapshot dir ref)
    set(_basalt_snapshot_ok FALSE)
    set(_basalt_snapshot_changed TRUE)
    set(_basalt_snapshot_head "")
    if(EXISTS "${dir}/.git")
        execute_process(
            COMMAND git -C "${dir}" rev-parse HEAD
            OUTPUT_VARIABLE _basalt_snapshot_head OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _basalt_snapshot_res ERROR_QUIET
        )
        if(NOT _basalt_snapshot_res EQUAL 0)
            set(_basalt_snapshot_head "")
        endif()
    else()
        execute_process(COMMAND git init --quiet "${dir}" RESULT_VARIABLE _basalt_snapshot_res ERROR_QUIET)
        if(_basalt_snapshot_res EQUAL 0)
            execute_process(
                COMMAND git -C "${dir}" remote add origin https://github.com/microsoft/vcpkg.git
                RESULT_VARIABLE _basalt_snapshot_res OUTPUT_QUIET ERROR_QUIET
            )
        endif()
    endif()
    if(_basalt_snapshot_res EQUAL 0)
        execute_process(
            COMMAND git -C "${dir}" fetch --depth=1 origin "${ref}"
            RESULT_VARIABLE _basalt_snapshot_res OUTPUT_QUIET
        )
    endif()
    if(_basalt_snapshot_res EQUAL 0)
        execute_process(
            COMMAND git -C "${dir}" rev-parse FETCH_HEAD
            OUTPUT_VARIABLE _basalt_snapshot_target OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _basalt_snapshot_res ERROR_QUIET
        )
    endif()
    if(_basalt_snapshot_res EQUAL 0)
        if(_basalt_snapshot_head STREQUAL _basalt_snapshot_target)
            set(_basalt_snapshot_ok TRUE)
            set(_basalt_snapshot_changed FALSE)
        else()
            execute_process(
                COMMAND git -C "${dir}" -c advice.detachedHead=false checkout --quiet --detach "${_basalt_snapshot_target}"
                RESULT_VARIABLE _basalt_snapshot_res OUTPUT_QUIET
            )
            if(_basalt_snapshot_res EQUAL 0)
                set(_basalt_snapshot_ok TRUE)
            endif()
        endif()
    endif()
endmacro()

# Locate a vcpkg checkout, tracking whether it belongs to the user.
set(_vcpkg_candidates)
set(_vcpkg_candidate_owners)
if(DEFINED VCPKG_ROOT AND NOT "${VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "${VCPKG_ROOT}")
    list(APPEND _vcpkg_candidate_owners TRUE)
endif()
if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{VCPKG_ROOT}")
    list(APPEND _vcpkg_candidate_owners TRUE)
endif()
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT} AND NOT "$ENV{VCPKG_INSTALLATION_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{VCPKG_INSTALLATION_ROOT}")
    list(APPEND _vcpkg_candidate_owners FALSE)
endif()
if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{HOME}/vcpkg" "$ENV{HOME}/.cache/basalt-vcpkg")
    list(APPEND _vcpkg_candidate_owners TRUE FALSE)
endif()
if(DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/vcpkg" _userprofile_vcpkg)
    list(APPEND _vcpkg_candidates "${_userprofile_vcpkg}")
    list(APPEND _vcpkg_candidate_owners TRUE)
endif()
list(APPEND _vcpkg_candidates "C:/vcpkg" "/usr/local/share/vcpkg" "${CMAKE_BINARY_DIR}/_vcpkg")
list(APPEND _vcpkg_candidate_owners FALSE FALSE FALSE)

set(_basalt_vcpkg_root "")
set(_basalt_root_user_owned FALSE)
set(_basalt_candidate_index 0)
foreach(_cand IN LISTS _vcpkg_candidates)
    list(GET _vcpkg_candidate_owners ${_basalt_candidate_index} _basalt_root_user_owned)
    math(EXPR _basalt_candidate_index "${_basalt_candidate_index} + 1")
    if(EXISTS "${_cand}/scripts/buildsystems/vcpkg.cmake")
        set(_basalt_vcpkg_root "${_cand}")
        break()
    endif()
endforeach()

# A checkout that predates the baseline resolves against a stale version database
# and a stale tool, so realign managed checkouts and fall back to a private snapshot.
set(_basalt_vcpkg_rebootstrap FALSE)
if(_basalt_vcpkg_root AND _basalt_baseline)
    execute_process(
        COMMAND git -C "${_basalt_vcpkg_root}" merge-base --is-ancestor "${_basalt_baseline}" HEAD
        RESULT_VARIABLE _basalt_root_current ERROR_QUIET OUTPUT_QUIET
    )
    if(NOT _basalt_root_current EQUAL 0)
        if(_basalt_root_user_owned)
            message(WARNING
                "The vcpkg checkout at ${_basalt_vcpkg_root} predates the pinned baseline ${_basalt_baseline}; "
                "using a private snapshot instead. Update the checkout to use it again.")
            set(_basalt_vcpkg_root "")
        else()
            execute_process(
                COMMAND git -C "${_basalt_vcpkg_root}" cat-file -e "${_basalt_baseline}^{commit}"
                RESULT_VARIABLE _basalt_has_baseline ERROR_QUIET
            )
            if(NOT _basalt_has_baseline EQUAL 0)
                message(STATUS "Fetching vcpkg baseline ${_basalt_baseline}...")
                execute_process(
                    COMMAND git -C "${_basalt_vcpkg_root}" fetch --depth=1 origin "${_basalt_baseline}"
                    RESULT_VARIABLE _basalt_has_baseline OUTPUT_QUIET
                )
            endif()
            if(_basalt_has_baseline EQUAL 0)
                execute_process(
                    COMMAND git -C "${_basalt_vcpkg_root}" -c advice.detachedHead=false checkout --quiet --detach "${_basalt_baseline}"
                    RESULT_VARIABLE _basalt_checkout_res OUTPUT_QUIET
                )
                if(_basalt_checkout_res EQUAL 0)
                    set(_basalt_root_current 0)
                    set(_basalt_vcpkg_rebootstrap TRUE)
                endif()
            endif()
            if(NOT _basalt_root_current EQUAL 0)
                message(WARNING
                    "Could not align ${_basalt_vcpkg_root} with the baseline ${_basalt_baseline}; "
                    "using a private snapshot instead.")
                set(_basalt_vcpkg_root "")
            endif()
        endif()
    endif()
endif()

if(NOT _basalt_vcpkg_root)
    if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(_basalt_vcpkg_root "$ENV{HOME}/.cache/basalt-vcpkg")
    else()
        set(_basalt_vcpkg_root "${CMAKE_BINARY_DIR}/_vcpkg")
    endif()
    message(STATUS "Preparing the vcpkg snapshot ${_basalt_baseline_ref} in ${_basalt_vcpkg_root}...")
    basalt_vcpkg_snapshot("${_basalt_vcpkg_root}" "${_basalt_baseline_ref}")
    if(NOT _basalt_snapshot_ok)
        message(FATAL_ERROR "Failed to prepare a vcpkg checkout in ${_basalt_vcpkg_root}")
    endif()
    if(_basalt_snapshot_changed)
        set(_basalt_vcpkg_rebootstrap TRUE)
    endif()
endif()

# Ensure the vcpkg binary exists and matches the checkout.
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    set(_vcpkg_exe "${_basalt_vcpkg_root}/vcpkg.exe")
else()
    set(_vcpkg_exe "${_basalt_vcpkg_root}/vcpkg")
endif()

if(_basalt_vcpkg_rebootstrap AND EXISTS "${_vcpkg_exe}")
    file(REMOVE "${_vcpkg_exe}")
endif()

if(NOT EXISTS "${_vcpkg_exe}" AND NOT _basalt_vcpkg_rebootstrap)
    find_program(_system_vcpkg vcpkg)
    if(_system_vcpkg)
        file(CREATE_LINK "${_system_vcpkg}" "${_vcpkg_exe}" COPY_ON_ERROR SYMBOLIC)
    endif()
endif()

if(NOT EXISTS "${_vcpkg_exe}")
    message(STATUS "Bootstrapping vcpkg in ${_basalt_vcpkg_root}...")
    # The tool download flakes now and then; give the bootstrap a few attempts.
    set(_boot_res 1)
    set(_boot_attempt 0)
    while(NOT _boot_res EQUAL 0 AND _boot_attempt LESS 3)
        math(EXPR _boot_attempt "${_boot_attempt} + 1")
        if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
            execute_process(
                COMMAND cmd /c "${_basalt_vcpkg_root}/bootstrap-vcpkg.bat" -disableMetrics
                WORKING_DIRECTORY "${_basalt_vcpkg_root}"
                RESULT_VARIABLE _boot_res
            )
        else()
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E env sh "${_basalt_vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
                WORKING_DIRECTORY "${_basalt_vcpkg_root}"
                RESULT_VARIABLE _boot_res
            )
        endif()
        if(NOT _boot_res EQUAL 0 AND _boot_attempt LESS 3)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 15)
        endif()
    endwhile()
    if(NOT _boot_res EQUAL 0)
        message(FATAL_ERROR "Failed to bootstrap vcpkg in ${_basalt_vcpkg_root}")
    endif()
endif()

set(VCPKG_ROOT "${_basalt_vcpkg_root}" CACHE PATH "Path to vcpkg root" FORCE)
set(ENV{VCPKG_ROOT} "${_basalt_vcpkg_root}")

set(_vcpkg_toolchain "${_basalt_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "" AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "${_vcpkg_toolchain}")
    if(NOT "${CMAKE_TOOLCHAIN_FILE}" MATCHES "vcpkg\\.cmake$")
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH "Chainloaded toolchain file" FORCE)
    endif()
endif()
set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file" FORCE)
