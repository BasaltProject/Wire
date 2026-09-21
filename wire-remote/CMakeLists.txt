basalt_module(wire-remote
    SOURCES
        src/ApiToken.cpp
        src/AttemptTracker.cpp
        src/Http.cpp
        src/HttpServer.cpp
        src/QueryServer.cpp
        src/UpdateChecker.cpp
    DEPENDS basalt::wire-auth basalt::wire-common basalt::wire-io
    PRIVATE_DEPENDS basalt::mbedtls
)



