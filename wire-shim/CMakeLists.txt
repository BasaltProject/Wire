# The embedded Java machine (Basalt Shim) loaded through JNI. Added only when a JDK with jni.h is found.
basalt_module(wire-shim
    SOURCES
        src/ShimHost.cpp
)
target_include_directories(wire-shim SYSTEM PUBLIC ${BASALT_JNI_INCLUDE_DIR} ${BASALT_JNI_INCLUDE_DIR}/linux
                                                   ${BASALT_JNI_INCLUDE_DIR}/darwin ${BASALT_JNI_INCLUDE_DIR}/win32
                                                   ${BASALT_JNI_INCLUDE_DIR}/freebsd)
target_link_libraries(wire-shim PUBLIC ${CMAKE_DL_LIBS})
