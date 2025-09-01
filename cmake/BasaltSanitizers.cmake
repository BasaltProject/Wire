# Sanitizer selection. BASALT_SANITIZE is a comma-separated list: address, undefined, thread.

add_library(basalt_sanitizers INTERFACE)
add_library(basalt::sanitizers ALIAS basalt_sanitizers)

if(BASALT_SANITIZE)
    string(REPLACE "," ";" _basalt_sanitizers "${BASALT_SANITIZE}")
    if("thread" IN_LIST _basalt_sanitizers AND "address" IN_LIST _basalt_sanitizers)
        message(FATAL_ERROR "BASALT_SANITIZE: thread and address are mutually exclusive")
    endif()
    string(REPLACE ";" "," _basalt_sanitize_flag "${_basalt_sanitizers}")
    target_compile_options(basalt_sanitizers INTERFACE
        -fsanitize=${_basalt_sanitize_flag}
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    target_link_options(basalt_sanitizers INTERFACE -fsanitize=${_basalt_sanitize_flag})
    message(STATUS "Basalt: sanitizers enabled (${_basalt_sanitize_flag})")
endif()
