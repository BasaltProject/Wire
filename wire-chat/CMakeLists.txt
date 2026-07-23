basalt_module(wire-chat
    SOURCES
        src/ChatMessage.cpp
        src/LastSeenValidator.cpp
        src/SecureChat.cpp
    DEPENDS basalt::wire-common basalt::wire-text basalt::wire-protocol basalt::wire-auth
)

basalt_test(wire-chat-test
    SOURCES
        test/SecureChatTest.cpp
    DEPENDS basalt::wire-chat
)
