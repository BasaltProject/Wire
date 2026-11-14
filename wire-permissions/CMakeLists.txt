basalt_module(wire-permissions
    SOURCES
        src/PermissionChain.cpp
)

basalt_test(wire-permissions-chain-test
    SOURCES
        test/PermissionChainTest.cpp
    DEPENDS basalt::wire-permissions
)
