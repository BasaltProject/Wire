include(BasaltWasm)
include(BasaltWasmAbi)

basalt_wasm_abi_headers(${CMAKE_CURRENT_SOURCE_DIR}/abi _abi_headers)
basalt_wasm_abi_rows(_abi_rows ${_abi_headers})
list(LENGTH _abi_rows _abi_count)
if(_abi_count LESS 10)
    message(FATAL_ERROR "the ABI headers yielded ${_abi_count} imports, the parser is out of step")
endif()
string(CONCAT _abi_source "#include \"Internal.hpp\"\n\nnamespace basalt::wire::wasm::detail {\nnamespace {\n"
    "constexpr AbiFunction kGenerated[] = {\n")
foreach(_row ${_abi_rows})
    string(REPLACE "|" ";" _f "${_row}")
    list(GET _f 0 _module)
    list(GET _f 1 _name)
    list(GET _f 2 _params)
    list(GET _f 3 _result)
    list(GET _f 4 _scope)
    if(_result STREQUAL "v")
        set(_result "")
    endif()
    if(_module STREQUAL "basalt")
        set(_module kBasaltModule)
    else()
        set(_module kProxyModule)
    endif()
    string(SUBSTRING ${_scope} 0 1 _first)
    string(TOUPPER ${_first} _first)
    string(SUBSTRING ${_scope} 1 -1 _rest)
    string(APPEND _abi_source "    {${_module}, \"${_name}\", \"${_params}>${_result}\", AbiScope::${_first}${_rest}},\n")
endforeach()
string(APPEND _abi_source "};\n} // namespace\n\nstd::span<const AbiFunction> generatedAbiFunctions() { return kGenerated; }\n\n}\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/AbiGenerated.cpp.tmp "${_abi_source}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/AbiGenerated.cpp.tmp
    ${CMAKE_CURRENT_BINARY_DIR}/AbiGenerated.cpp)

if(BASALT_WASM_BACKEND_RESOLVED STREQUAL "wasmtime")
    basalt_wasmtime_import()
    set(_backend_sources src/wasmtime/WasmtimeBackend.cpp)
    set(_backend_library basalt::wasmtime)
    set(_backend_id "wasmtime-${BASALT_WASMTIME_VERSION}")
elseif(BASALT_WASM_BACKEND_RESOLVED STREQUAL "wamr")
    add_subdirectory(wamr)
    set(_backend_sources src/wamr/WamrBackend.cpp)
    set(_backend_library basalt::wamr)
    set(_backend_id "wamr-${BASALT_WAMR_VERSION}")
else()
    message(FATAL_ERROR "BASALT_WASM_BACKEND must be auto, wasmtime or wamr, not '${BASALT_WASM_BACKEND}'")
endif()

basalt_module(wire-wasm
    SOURCES
        src/Abi.cpp
        src/Bind.cpp
        src/Cache.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/AbiGenerated.cpp
        src/Validate.cpp
        src/Value.cpp
        ${_backend_sources}
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS ${_backend_library}
)
target_compile_definitions(wire-wasm PRIVATE BASALT_WASM_BACKEND_ID="${_backend_id}")
target_include_directories(wire-wasm PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/abi>)
set(BASALT_WASM_BACKEND_ID "${_backend_id}" PARENT_SCOPE)

basalt_test(wire-wasm-test
    SOURCES
        test/WasmTest.cpp
        test/ValueTest.cpp
    DEPENDS basalt::wire-wasm
)

basalt_bench(wire-wasm-call-bench
    SOURCES
        bench/CallBench.cpp
    DEPENDS basalt::wire-wasm
)

option(BASALT_BUILD_TOOLS "Build the developer tools that are not part of the server" OFF)
if(BASALT_BUILD_TOOLS)
    add_subdirectory(tools/trace)
endif()
