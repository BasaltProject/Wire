basalt_module(wire-sqlite
    SOURCES
        src/Migrations.cpp
        src/Sqlite.cpp
    PRIVATE_DEPENDS basalt::sqlite
)

basalt_test(wire-sqlite-test
    SOURCES
        test/SqliteTest.cpp
    DEPENDS basalt::wire-sqlite
)
