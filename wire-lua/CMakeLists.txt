basalt_module(wire-lua
    SOURCES
        src/LuaArgs.cpp
        src/LuaCall.cpp
        src/LuaRef.cpp
        src/LuaFailure.cpp
        src/LuaSandbox.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::lua
)

basalt_test(wire-lua-call-test
    SOURCES
        test/LuaCallTest.cpp
    DEPENDS basalt::wire-lua
)

basalt_test(wire-lua-test
    SOURCES
        test/LuaTest.cpp
    DEPENDS basalt::wire-lua
)
