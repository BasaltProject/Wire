# Configures vcpkg manifest mode, overlay ports/triplets, and target triplet before project().

if(DEFINED BASALT_VCPKG_CONFIGURED)
    return()
endif()
set(BASALT_VCPKG_CONFIGURED TRUE)

set(_basalt_wire_root "${CMAKE_CURRENT_LIST_DIR}/..")
cmake_path(NORMAL_PATH _basalt_wire_root)

set(VCPKG_OVERLAY_PORTS "${_basalt_wire_root}/vcpkg-overlays/ports" CACHE STRING "Basalt vcpkg overlay ports" FORCE)
set(VCPKG_OVERLAY_TRIPLETS "${_basalt_wire_root}/vcpkg-overlays/triplets" CACHE STRING "Basalt vcpkg overlay triplets" FORCE)

if(NOT DEFINED VCPKG_TARGET_TRIPLET OR "${VCPKG_TARGET_TRIPLET}" STREQUAL "")
    if(ANDROID OR DEFINED ANDROID_ABI OR "${CMAKE_TOOLCHAIN_FILE}" MATCHES "android\\.toolchain\\.cmake")
        if("${ANDROID_ABI}" STREQUAL "x86_64")
            set(_basalt_triplet "x64-android")
        else()
            set(_basalt_triplet "arm64-android")
        endif()
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD" OR "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "FreeBSD")
        if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$" OR (NOT DEFINED CMAKE_SYSTEM_PROCESSOR AND "${CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$"))
            set(_basalt_triplet "arm64-freebsd")
        else()
            set(_basalt_triplet "x64-freebsd")
        endif()
    elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin" OR APPLE)
        if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64" OR (NOT DEFINED CMAKE_OSX_ARCHITECTURES AND "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "x86_64"))
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
        if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$" OR (NOT DEFINED CMAKE_SYSTEM_PROCESSOR AND "${CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "^(aarch64|arm64)$"))
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

# Locate or bootstrap VCPKG_ROOT.
set(_vcpkg_candidates)
if(DEFINED VCPKG_ROOT AND NOT "${VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "${VCPKG_ROOT}")
endif()
if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{VCPKG_ROOT}")
endif()
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT} AND NOT "$ENV{VCPKG_INSTALLATION_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{VCPKG_INSTALLATION_ROOT}")
endif()
if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND _vcpkg_candidates "$ENV{HOME}/vcpkg" "$ENV{HOME}/.cache/basalt-vcpkg")
endif()
if(DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/vcpkg" _userprofile_vcpkg)
    list(APPEND _vcpkg_candidates "${_userprofile_vcpkg}")
endif()
list(APPEND _vcpkg_candidates "C:/vcpkg" "/usr/local/share/vcpkg" "${CMAKE_BINARY_DIR}/_vcpkg")

set(_basalt_vcpkg_root "")
foreach(_cand IN LISTS _vcpkg_candidates)
    if(EXISTS "${_cand}/scripts/buildsystems/vcpkg.cmake")
        set(_basalt_vcpkg_root "${_cand}")
        break()
    endif()
endforeach()

if(NOT _basalt_vcpkg_root)
    if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(_basalt_vcpkg_root "$ENV{HOME}/.cache/basalt-vcpkg")
    else()
        set(_basalt_vcpkg_root "${CMAKE_BINARY_DIR}/_vcpkg")
    endif()
    message(STATUS "Cloning vcpkg into ${_basalt_vcpkg_root}...")
    execute_process(
        COMMAND git clone --filter=blob:none https://github.com/microsoft/vcpkg.git "${_basalt_vcpkg_root}"
        RESULT_VARIABLE _clone_res
    )
    if(NOT _clone_res EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg repository into ${_basalt_vcpkg_root}")
    endif()
endif()

# Ensure the builtin-baseline commit is present in the local vcpkg repository.
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
    file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" _manifest_json)
    string(JSON _baseline ERROR_VARIABLE _json_err GET "${_manifest_json}" "builtin-baseline")
    if(_baseline AND NOT _json_err)
        execute_process(
            COMMAND git -C "${_basalt_vcpkg_root}" cat-file -e "${_baseline}^{commit}"
            RESULT_VARIABLE _has_baseline
            ERROR_QUIET
        )
        if(NOT _has_baseline EQUAL 0)
            message(STATUS "Fetching vcpkg baseline ${_baseline}...")
            execute_process(
                COMMAND git -C "${_basalt_vcpkg_root}" fetch --depth=1 origin "${_baseline}"
                RESULT_VARIABLE _fetch_res
            )
        endif()
    endif()
endif()

# Ensure the vcpkg binary exists in _basalt_vcpkg_root.
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    set(_vcpkg_exe "${_basalt_vcpkg_root}/vcpkg.exe")
else()
    set(_vcpkg_exe "${_basalt_vcpkg_root}/vcpkg")
endif()

if(NOT EXISTS "${_vcpkg_exe}")
    find_program(_system_vcpkg vcpkg)
    if(_system_vcpkg)
        file(CREATE_LINK "${_system_vcpkg}" "${_vcpkg_exe}" COPY_ON_ERROR SYMBOLIC)
    else()
        message(STATUS "Bootstrapping vcpkg in ${_basalt_vcpkg_root}...")
        if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
            execute_process(
                COMMAND cmd /c "${_basalt_vcpkg_root}/bootstrap-vcpkg.bat" -disableMetrics
                WORKING_DIRECTORY "${_basalt_vcpkg_root}"
                RESULT_VARIABLE _boot_res
            )
        else()
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E env VCPKG_FORCE_SYSTEM_BINARIES=1 sh "${_basalt_vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
                WORKING_DIRECTORY "${_basalt_vcpkg_root}"
                RESULT_VARIABLE _boot_res
            )
        endif()
        if(NOT _boot_res EQUAL 0)
            message(FATAL_ERROR "Failed to bootstrap vcpkg in ${_basalt_vcpkg_root}")
        endif()
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
