cmake_minimum_required(VERSION 3.16)
project(qagame C)

# Build option: DLL or QVM
option(BUILD_QVM "Compile qagame as Q3VM bytecode instead of native DLL" OFF)

# Architecture string — matches Q3 DLL naming convention
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_STRING "x86_64")
else()
    set(ARCH_STRING "x86")
endif()

set(GAME_SOURCES_BASE
    # g_main.c must come first: vmMain() must be the first function in the
    # first compiled source file, since the QVM interpreter always calls
    # address 0 as the entry point.
    code/game/g_main.c

    # Background (shared physics/items/misc — no engine calls)
    code/game/bg_misc.c
    code/game/bg_pmove.c
    code/game/bg_slidemove.c
    code/game/bg_lib.c

    # Game logic
    code/game/g_active.c
    code/game/g_arenas.c
    code/game/g_bot_stub.c
    code/game/g_client.c
    code/game/g_cmds.c
    code/game/g_combat.c
    code/game/g_items.c
    code/game/g_mem.c
    code/game/g_misc.c
    code/game/g_missile.c
    code/game/g_mover.c
    code/game/g_session.c
    code/game/g_spawn.c
    code/game/g_svcmds.c
    code/game/g_defrag.c
    code/game/g_target.c
    code/game/g_team.c
    code/game/g_trigger.c
    code/game/g_utils.c
    code/game/g_weapon.c

    # Shared utilities compiled into the game module
    code/qcommon/q_shared.c
    code/qcommon/q_math.c
)

if(BUILD_QVM)
    message(STATUS "Building VMs as Q3VM bytecode")

    # Find Python for pk3 creation
    find_program(PYTHON_EXECUTABLE python3 python
        REQUIRED
    )
    message(STATUS "Found Python: ${PYTHON_EXECUTABLE}")

    # Find Q3VM compilation tools in bin directory
    find_program(Q3LCC q3lcc
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        REQUIRED
    )
    find_program(Q3ASM q3asm
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        REQUIRED
    )

    message(STATUS "Found q3lcc: ${Q3LCC}")
    message(STATUS "Found q3asm: ${Q3ASM}")

    # Output directories for QVM files and intermediate ASM
    set(VM_OUTPUT_DIR "${CMAKE_BINARY_DIR}/vm")
    set(QVM_ASM_DIR "${CMAKE_BINARY_DIR}/qvm.dir")
    file(MAKE_DIRECTORY "${VM_OUTPUT_DIR}" "${QVM_ASM_DIR}")

    # Macro to build a QVM module
    macro(build_qvm MODULE_NAME MODULE_SOURCES MODULE_INCLUDE MODULE_SYSCALLS)
        # Convert relative source paths to absolute
        set(${MODULE_NAME}_SOURCES_ABSOLUTE "")
        set(${MODULE_NAME}_ASM_FILES "")
        foreach(SOURCE ${MODULE_SOURCES})
            list(APPEND ${MODULE_NAME}_SOURCES_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}")
            get_filename_component(BASE_NAME "${SOURCE}" NAME_WE)
            list(APPEND ${MODULE_NAME}_ASM_FILES "${QVM_ASM_DIR}/${BASE_NAME}.asm")
        endforeach()
        # Add syscalls.asm to the list
        list(APPEND ${MODULE_NAME}_ASM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_SYSCALLS}")

        # Build <module>.qvm
        add_custom_target(${MODULE_NAME} ALL
            COMMAND ${Q3LCC}
                -S
                -DQ3_VM
                -I${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_INCLUDE}
                -I${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon
                -I${CMAKE_CURRENT_SOURCE_DIR}/code/renderercommon
                -I${CMAKE_CURRENT_SOURCE_DIR}/code/client
                -I${CMAKE_CURRENT_SOURCE_DIR}/code/ui
                ${${MODULE_NAME}_SOURCES_ABSOLUTE}
            COMMAND ${CMAKE_COMMAND} -E echo "Assembling ${MODULE_NAME}.qvm..."
            COMMAND ${Q3ASM}
                -vq3  # VM_MAGIC v1 (Q3 1.32b) format: cgame-proxymod / older loaders reject VM_MAGIC_VER2
                -o ${VM_OUTPUT_DIR}/${MODULE_NAME}.qvm
                ${${MODULE_NAME}_ASM_FILES}
            WORKING_DIRECTORY "${QVM_ASM_DIR}"
            COMMENT "Compiling ${MODULE_NAME}.qvm with q3lcc and q3asm"
        )
    endmacro()

    # Build qagame.qvm
    build_qvm(qagame
        "${GAME_SOURCES_BASE}"
        "code/game"
        "code/game/g_syscalls.asm"
    )

    # Build cgame.qvm
    set(CGAME_SOURCES
        code/cgame/cg_main.c
        code/cgame/cg_consolecmds.c
        code/cgame/cg_defrag.c
        code/cgame/cg_dfrecords.c
        code/cgame/cg_dfhud.c
        code/cgame/cg_dfaccel.c
        code/cgame/cg_dfautorec.c
        code/cgame/cg_dfchs.c
        code/cgame/cg_dfdemocam.c
        code/cgame/cg_dfghost.c
        code/cgame/cg_dfob.c
        code/cgame/cg_dfreplay.c
        code/cgame/cg_draw.c
        code/cgame/cg_drawtools.c
        code/cgame/cg_effects.c
        code/cgame/cg_ents.c
        code/cgame/cg_event.c
        code/cgame/cg_info.c
        code/cgame/cg_localents.c
        code/cgame/cg_marks.c
        code/cgame/cg_particles.c
        code/cgame/cg_players.c
        code/cgame/cg_playerstate.c
        code/cgame/cg_predict.c
        code/cgame/cg_scoreboard.c
        code/cgame/cg_servercmds.c
        code/cgame/cg_snapshot.c
        code/cgame/cg_view.c
        code/cgame/cg_weapons.c
        code/game/bg_misc.c
        code/game/bg_pmove.c
        code/game/bg_slidemove.c
        code/game/bg_lib.c
        code/qcommon/q_shared.c
        code/qcommon/q_math.c
    )
    build_qvm(cgame
        "${CGAME_SOURCES}"
        "code/cgame"
        "code/cgame/cg_syscalls.asm"
    )

    # Build ui.qvm
    set(UI_SOURCES
        # ui_main.c must come first: vmMain() must be the first function in
        # the first compiled source file, since the QVM interpreter always
        # calls address 0 as the entry point.
        code/ui/ui_main.c
        code/game/bg_misc.c
        code/game/bg_pmove.c
        code/game/bg_slidemove.c
        code/game/bg_lib.c
        code/ui/ui_atoms.c
        code/ui/ui_gameinfo.c
        code/ui/ui_players.c
        code/ui/ui_shared.c
        code/qcommon/q_shared.c
        code/qcommon/q_math.c
    )
    build_qvm(ui
        "${UI_SOURCES}"
        "code/ui"
        "code/ui/ui_syscalls.asm"
    )

    message(STATUS "QVM build configured.")
    message(STATUS "  Output: ${VM_OUTPUT_DIR}/qagame.qvm")
    message(STATUS "          ${VM_OUTPUT_DIR}/cgame.qvm")
    message(STATUS "          ${VM_OUTPUT_DIR}/ui.qvm")

else()
    # DLL compilation: use syscalls.c for native DLL
    message(STATUS "Building qagame as native DLL")

    set(GAME_SOURCES
        ${GAME_SOURCES_BASE}
        code/game/g_syscalls.c   # DLL entry point + trap implementations
    )

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

endif()

#############################################################################
# QVM Compilation
#############################################################################
# Enable with: cmake -DBUILD_QVM=ON ..
# Output: vm/qagame.qvm (Q3VM bytecode)
#
# Pipeline:
#   1. q3lcc (C compiler for Q3VM) — compiles C → Q3VM assembly
#   2. q3asm (Q3VM assembler) — assembles ASM → .qvm bytecode
#
# Tools required in bin/ directory:
#   - q3lcc    (from https://github.com/ec-/q3lcc)
#   - q3asm    (from https://github.com/ec-/q3asm)
#
# Source files:
#   - code/game/g_syscalls.asm — Q3VM syscall interface (required)
#   - All .c files in GAME_SOURCES_BASE — compiled to Q3VM bytecode
#
# Benefits of QVM mode:
#   - qagame.qvm + cgame.qvm share Q3VM address space
#   - Direct memory writes to cgame VM memory (0x11b000, etc.)
#   - Timer HUD works with original DeFRaG cgame.qvm
#   - Exact DeFRaG feature parity via shared-memory communication
