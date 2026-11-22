basalt_wasm_host_registry(_trace_hosts
    DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX Host
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/HostRegistry.cpp
    INCLUDES HostApi.hpp
    NAMESPACE basalt::wire::wasm::trace
    ENTRY "std::vector<HostImport> buildImports(World& world)"
    DECLARE "void add@Area@Imports(World& world, std::vector<HostImport>& out)"
    CALL "add@Area@Imports(world, imports)"
    BODY_BEGIN "std::vector<HostImport> imports"
    BODY_END "return imports")

add_executable(basalt-wasm-trace
    src/main.cpp
    src/Scenario.cpp
    src/Text.cpp
    src/World.cpp
    ${_trace_hosts}
)
target_include_directories(basalt-wasm-trace PRIVATE src)
target_link_libraries(basalt-wasm-trace PRIVATE basalt::wire-wasm basalt::wire-plugins basalt::wire-common
    basalt::warnings basalt::sanitizers basalt::parity basalt::platform)
set_target_properties(basalt-wasm-trace PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
