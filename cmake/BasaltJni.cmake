# Finds the JDK's jni.h for the modules that embed Basalt Shim. BASALT_JNI_INCLUDE_DIR is empty when there is none.
set(BASALT_JNI_INCLUDE_DIR "" CACHE PATH "Directory that holds jni.h (a JDK include directory)")
if(NOT BASALT_JNI_INCLUDE_DIR)
    file(GLOB _basalt_jdks /usr/lib/jvm/*/include /usr/local/openjdk*/include
         /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
    find_path(_basalt_jni jni.h HINTS $ENV{JAVA_HOME}/include ${_basalt_jdks} NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
    if(_basalt_jni)
        set(BASALT_JNI_INCLUDE_DIR "${_basalt_jni}" CACHE PATH "" FORCE)
    endif()
endif()
