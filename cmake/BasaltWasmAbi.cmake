# Derives the import table of the plugin ABI from the normative headers, so no table is written by hand.
#
# The headers are wire-wasm/abi/basalt_abi.h (the core), every wire-wasm/abi/areas/*.h and basalt_proxy_abi.h. An
# import is declared with one of three macros followed by the C prototype:
#
#   BASALT_IMPORT(name)         module "basalt", server profile only          function basalt_<name>
#   BASALT_IMPORT_SHARED(name)  module "basalt", server and proxy profiles    function basalt_<name>
#   BASALT_PROXY_IMPORT(name)   module "basalt_proxy", proxy profile only     function basalt_proxy_<name>
#
# Parameters are int32_t, int64_t, float, double or any pointer (an i32 offset); results are void, int32_t, int64_t,
# float or double. A new area is a new header in areas/ and needs no edit anywhere in this file or in the tables.

# basalt_wasm_abi_headers(<abi dir> <out>): the headers in table order, core first, areas by name, proxy last.
function(basalt_wasm_abi_headers ABI_DIR OUT)
    file(GLOB areas CONFIGURE_DEPENDS ${ABI_DIR}/areas/*.h)
    list(SORT areas)
    set(${OUT} ${ABI_DIR}/basalt_abi.h ${areas} ${ABI_DIR}/basalt_proxy_abi.h PARENT_SCOPE)
endfunction()

function(basalt_wasm_abi_letter TYPE WHAT OUT)
    string(STRIP "${TYPE}" TYPE)
    if(TYPE MATCHES "\\*")
        set(letter i)
    elseif(TYPE MATCHES "^int32_t")
        set(letter i)
    elseif(TYPE MATCHES "^int64_t")
        set(letter l)
    elseif(TYPE MATCHES "^float")
        set(letter f)
    elseif(TYPE MATCHES "^double")
        set(letter d)
    elseif(TYPE STREQUAL "void")
        set(letter v)
    else()
        message(FATAL_ERROR "ABI header: unsupported ${WHAT} type '${TYPE}'")
    endif()
    set(${OUT} ${letter} PARENT_SCOPE)
endfunction()

# basalt_wasm_abi_rows(<out> <header>...): one "module|name|params|result|scope" entry per import, scope being
# server, shared or proxy and result being v for none.
function(basalt_wasm_abi_rows OUT)
    set(all "")
    set(macros BASALT_IMPORT_SHARED BASALT_IMPORT BASALT_PROXY_IMPORT)
    foreach(header ${ARGN})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${header})
        file(READ ${header} text)
        string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" text "${text}")
        set(expected 0)
        foreach(macro ${macros})
            string(REGEX MATCHALL "${macro}\\([a-z0-9_]+\\)" uses "${text}")
            string(REGEX MATCHALL "#define[ \t]+${macro}\\(" defines "${text}")
            list(LENGTH uses used)
            list(LENGTH defines defined)
            math(EXPR expected "${expected} + ${used} - ${defined}")
            if(macro STREQUAL "BASALT_IMPORT_SHARED")
                set(module basalt)
                set(prefix basalt_)
                set(scope shared)
            elseif(macro STREQUAL "BASALT_IMPORT")
                set(module basalt)
                set(prefix basalt_)
                set(scope server)
            else()
                set(module basalt_proxy)
                set(prefix basalt_proxy_)
                set(scope proxy)
            endif()
            string(REGEX MATCHALL "${macro}\\([a-z0-9_]+\\)[ \t\r\n]+[a-z0-9_]+[ \t\r\n]+${prefix}[a-z0-9_]+\\([^)]*\\)"
                prototypes "${text}")
            foreach(prototype ${prototypes})
                string(REGEX MATCH "${macro}\\(([a-z0-9_]+)\\)[ \t\r\n]+([a-z0-9_]+)[ \t\r\n]+${prefix}([a-z0-9_]+)\\(([^)]*)\\)"
                    ignored "${prototype}")
                set(name ${CMAKE_MATCH_1})
                set(returned ${CMAKE_MATCH_2})
                set(declared ${CMAKE_MATCH_3})
                if(NOT name STREQUAL declared)
                    message(FATAL_ERROR "${header}: ${macro}(${name}) declares ${prefix}${declared}")
                endif()
                string(REPLACE "," ";" parameterList "${CMAKE_MATCH_4}")
                set(params "")
                foreach(parameter ${parameterList})
                    string(STRIP "${parameter}" parameter)
                    if(parameter STREQUAL "void" OR parameter STREQUAL "")
                        continue()
                    endif()
                    basalt_wasm_abi_letter("${parameter}" "parameter of ${name}" letter)
                    string(APPEND params ${letter})
                endforeach()
                basalt_wasm_abi_letter("${returned}" "result of ${name}" result)
                list(APPEND all "${module}|${name}|${params}|${result}|${scope}")
                math(EXPR expected "${expected} - 1")
            endforeach()
        endforeach()
        if(NOT expected EQUAL 0)
            message(FATAL_ERROR "${header}: ${expected} import declaration(s) did not parse; use one prototype per "
                "BASALT_IMPORT*/BASALT_PROXY_IMPORT macro with the matching basalt_/basalt_proxy_ prefix")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES all)
    set(${OUT} "${all}" PARENT_SCOPE)
endfunction()

# basalt_wasm_abi_version(<out> <header>): the BASALT_ABI_VERSION of the core header.
function(basalt_wasm_abi_version OUT HEADER)
    file(READ ${HEADER} text)
    string(REGEX MATCH "#define BASALT_ABI_VERSION ([0-9]+)" ignored "${text}")
    set(${OUT} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()
