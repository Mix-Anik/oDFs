cmake_minimum_required(VERSION 3.16)
project(qagame C)

# Architecture string — matches Q3 DLL naming convention
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_STRING "x86_64")
else()
    set(ARCH_STRING "x86")
endif()

set(GAME_SOURCES
    # Background (shared physics/items/misc — no engine calls)
    code/game/bg_misc.c
    code/game/bg_pmove.c
    code/game/bg_slidemove.c

    # Game logic
    code/game/g_active.c
    code/game/g_arenas.c
    code/game/g_bot_stub.c
    code/game/g_client.c
    code/game/g_cmds.c
    code/game/g_combat.c
    code/game/g_items.c
    code/game/g_main.c
    code/game/g_mem.c
    code/game/g_misc.c
    code/game/g_missile.c
    code/game/g_mover.c
    code/game/g_session.c
    code/game/g_spawn.c
    code/game/g_svcmds.c
    code/game/g_syscalls.c   # DLL entry point + trap implementations
    code/game/g_target.c
    code/game/g_team.c
    code/game/g_trigger.c
    code/game/g_utils.c
    code/game/g_weapon.c

    # Shared utilities compiled into the game module
    code/qcommon/q_shared.c
    code/qcommon/q_math.c
)

# Excluded:
#   ai_*.c, g_bot.c    — require botlib (not included)
#   bg_lib.c           — QVM-only libc replacement, not needed for native DLL
#   g_syscalls.asm     — QVM assembler version of syscalls
#   g_rankings.c       — online rankings, not needed

add_library(qagame SHARED ${GAME_SOURCES})

target_include_directories(qagame PRIVATE
    code/game
    code/qcommon
)

set_target_properties(qagame PROPERTIES
    OUTPUT_NAME "qagame${ARCH_STRING}"
    PREFIX ""
)

if(MSVC)
    target_compile_options(qagame PRIVATE
        /W2         # moderate warnings
        /wd4018     # signed/unsigned mismatch (very common in Q3 code)
        /wd4244     # conversion, possible loss of data
        /wd4267     # size_t to int conversion
        /wd4305     # truncation from double to float
    )
    target_compile_definitions(qagame PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32
    )
else()
    target_compile_options(qagame PRIVATE
        -Wall
        -Wno-unused-variable
        -fvisibility=hidden
    )
endif()
