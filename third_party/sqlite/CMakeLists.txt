# SQLite 3.53.4, public domain, the official amalgamation as shipped by sqlite.org.
#
# Compiled in rather than linked against the host's: the cross-compilation sysroots for Linux and
# Windows carry no SQLite at all, so `-lsqlite3` fails outright on three of the five targets.
#
# The feature flags follow sqlite.org's own recommended build for an embedded application: what a
# player database needs is a file, a schema and prepared statements, and everything switched off
# below is either a shell feature or a compatibility path for a caller that does not exist here.

add_library(basalt_sqlite STATIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c)
add_library(basalt::sqlite ALIAS basalt_sqlite)

target_include_directories(basalt_sqlite SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(basalt_sqlite
    PRIVATE
        SQLITE_DQS=0
        SQLITE_THREADSAFE=1
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        SQLITE_LIKE_DOESNT_MATCH_BLOBS
        SQLITE_MAX_EXPR_DEPTH=0
        SQLITE_OMIT_DECLTYPE
        SQLITE_OMIT_DEPRECATED
        SQLITE_OMIT_PROGRESS_CALLBACK
        SQLITE_OMIT_SHARED_CACHE
        SQLITE_OMIT_AUTOINIT
        SQLITE_USE_ALLOCA
)
set_target_properties(basalt_sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(basalt_sqlite PRIVATE -w)
