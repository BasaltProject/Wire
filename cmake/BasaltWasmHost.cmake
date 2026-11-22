# basalt_wasm_host_registry(): lets a host implement one ABI area per file with no central list to edit.
#
# Every file DIR/<PREFIX><Area>.cpp is an area; a file named WasmImportsEntities.cpp (PREFIX WasmImports) is area
# "Entities". The function creates OUTPUT, a source that declares and calls one function per area, and puts the area
# sources plus OUTPUT in the variable OUT_SOURCES for the consumer to add to its module. Adding an area is adding a file:
# the glob re-runs the configure step by itself (CONFIGURE_DEPENDS).
#
#   basalt_wasm_host_registry(_imports
#       DIR       ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm
#       PREFIX    WasmImports
#       OUTPUT    ${CMAKE_CURRENT_BINARY_DIR}/WasmImportsRegistry.cpp
#       INCLUDES  WasmState.hpp                       (headers in DIR, or paths, the generated source needs)
#       NAMESPACE basalt::server::plugins::wasm
#       ENTRY     "void addAllImports(State& state, std::vector<abi::HostImport>& out)"
#       DECLARE   "void add@Area@Imports(State& state, std::vector<abi::HostImport>& out)"
#       CALL      "add@Area@Imports(state, out)")
#
# @Area@ is the area in PascalCase as in the file name and @area@ the same with a lowercase first letter. Optional
# BODY_BEGIN and BODY_END add statements before and after the calls, for an ENTRY that returns something. A semicolon
# is appended to DECLARE, CALL, BODY_BEGIN and BODY_END by the function, because CMake splits arguments at semicolons.
function(basalt_wasm_host_registry OUT_SOURCES)
    cmake_parse_arguments(ARG "" "DIR;PREFIX;OUTPUT;NAMESPACE;ENTRY;DECLARE;CALL;BODY_BEGIN;BODY_END" "INCLUDES" ${ARGN})
    file(GLOB areaSources CONFIGURE_DEPENDS ${ARG_DIR}/${ARG_PREFIX}[A-Z]*.cpp)
    list(SORT areaSources)
    set(includes "")
    foreach(include ${ARG_INCLUDES})
        if(EXISTS ${ARG_DIR}/${include})
            set(include ${ARG_DIR}/${include})
        endif()
        string(APPEND includes "#include \"${include}\"\n")
    endforeach()
    set(declarations "")
    set(calls "")
    foreach(source ${areaSources})
        get_filename_component(file ${source} NAME_WE)
        string(LENGTH "${ARG_PREFIX}" prefixLength)
        string(SUBSTRING ${file} ${prefixLength} -1 area)
        string(SUBSTRING ${area} 0 1 first)
        string(TOLOWER ${first} lowerFirst)
        string(SUBSTRING ${area} 1 -1 rest)
        set(camel ${lowerFirst}${rest})
        string(REPLACE "@Area@" ${area} declaration "${ARG_DECLARE}")
        string(REPLACE "@area@" ${camel} declaration "${declaration}")
        string(REPLACE "@Area@" ${area} call "${ARG_CALL}")
        string(REPLACE "@area@" ${camel} call "${call}")
        string(APPEND declarations "${declaration};\n")
        string(APPEND calls "    ${call};\n")
    endforeach()
    set(begin "")
    if(ARG_BODY_BEGIN)
        set(begin "    ${ARG_BODY_BEGIN};\n")
    endif()
    set(end "")
    if(ARG_BODY_END)
        set(end "    ${ARG_BODY_END};\n")
    endif()
    string(CONCAT text "${includes}\nnamespace ${ARG_NAMESPACE} {\n\n${declarations}\n${ARG_ENTRY} {\n"
        "${begin}${calls}${end}}\n\n}\n")
    file(WRITE ${ARG_OUTPUT}.tmp "${text}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARG_OUTPUT}.tmp ${ARG_OUTPUT})
    set(${OUT_SOURCES} ${areaSources} ${ARG_OUTPUT} PARENT_SCOPE)
endfunction()
