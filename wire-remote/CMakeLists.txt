basalt_module(wire-remote
    SOURCES
        src/ApiToken.cpp
        src/AttemptTracker.cpp
        src/Http.cpp
        src/HttpServer.cpp
    DEPENDS basalt::wire-common basalt::wire-io
    PRIVATE_DEPENDS basalt::mbedtls
)


