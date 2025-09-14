# basalt_embed_text(): turns text files into a module of string constants.
#
# Nothing reads a file from beside the binary at runtime: a Basalt release is one self-contained
# executable, and a folder of templates next to it is a folder somebody can lose. The templates are
# the files on disk during development and constants in the binary afterwards, so adding one is
# adding a file.
#
#   basalt_embed_text(server-runtime-config-defaults
#       NAMESPACE basalt::server::runtime::config
#       HEADER    basalt/server/runtime/config/ConfigDefaults.hpp
#       SUFFIX    DefaultToml
#       FILES     ${templates})
#
# `server_list.toml` becomes `kServerListDefaultToml`. The result is a static library the consumer
# lists in DEPENDS, so the generated header is visible exactly where the dependency is declared.

function(basalt_embed_text NAME)
    cmake_parse_arguments(ARG "" "NAMESPACE;HEADER;SUFFIX" "FILES" ${ARGN})

    if(NOT ARG_FILES)
        message(FATAL_ERROR "basalt_embed_text(${NAME}): FILES is required")
    endif()
    if(NOT ARG_HEADER)
        message(FATAL_ERROR "basalt_embed_text(${NAME}): HEADER is required")
    endif()
    if(NOT ARG_NAMESPACE)
        message(FATAL_ERROR "basalt_embed_text(${NAME}): NAMESPACE is required")
    endif()

    set(generated ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(header ${generated}/include/${ARG_HEADER})
    set(source ${generated}/${NAME}.cpp)

    add_custom_command(
        OUTPUT ${header} ${source}
        COMMAND ${CMAKE_COMMAND}
                -DOUT_HEADER=${header}
                -DOUT_SOURCE=${source}
                -DHEADER_INCLUDE=${ARG_HEADER}
                -DTEXT_NAMESPACE=${ARG_NAMESPACE}
                -DNAME_SUFFIX=${ARG_SUFFIX}
                "-DTEXT_FILES=${ARG_FILES}"
                -P ${BASALT_EMBED_SCRIPT}
        DEPENDS ${ARG_FILES} ${BASALT_EMBED_SCRIPT}
        COMMENT "Embedding ${NAME}"
        VERBATIM
    )

    add_library(${NAME} STATIC ${source} ${header})
    add_library(basalt::${NAME} ALIAS ${NAME})
    target_include_directories(${NAME} PUBLIC $<BUILD_INTERFACE:${generated}/include>)
    target_link_libraries(${NAME} PRIVATE basalt::warnings basalt::sanitizers basalt::parity)
    set_target_properties(${NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
endfunction()
