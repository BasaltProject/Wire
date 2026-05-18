basalt_module(wire-plugins
    SOURCES
        src/CommandSpec.cpp
        src/EventPriority.cpp
        src/PluginFolder.cpp
        src/PluginManifest.cpp
        src/PluginOrder.cpp
        src/PluginPackage.cpp
        src/PluginSignature.cpp
        src/PluginText.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-lua
    PRIVATE_DEPENDS basalt::monocypher
)

basalt_test(wire-plugins-package-test
    SOURCES
        test/PluginPackageTest.cpp
    DEPENDS basalt::wire-plugins
)

if(TARGET wire-plugins-package-test)
    target_compile_definitions(wire-plugins-package-test
        PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )
endif()

basalt_test(wire-plugins-signature-test
    SOURCES
        test/PluginSignatureTest.cpp
    DEPENDS basalt::wire-plugins
)

if(TARGET wire-plugins-signature-test)
    target_compile_definitions(wire-plugins-signature-test
        PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )
endif()

basalt_test(wire-plugins-order-test
    SOURCES
        test/PluginOrderTest.cpp
    DEPENDS basalt::wire-plugins
)

basalt_test(wire-plugins-manifest-test
    SOURCES
        test/PluginManifestTest.cpp
    DEPENDS basalt::wire-plugins
)

basalt_test(wire-plugins-command-spec-test
    SOURCES
        test/CommandSpecTest.cpp
    DEPENDS basalt::wire-plugins
)
