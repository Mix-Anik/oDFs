cmake_minimum_required(VERSION 3.10)
project(oDFs_testing C)

# dfgolden: the golden file tool. `dfgolden test` replays golden files through the game QVM (what the CI runs);
# `dfgolden make` turns demos into golden files; `dfgolden check` / `info` inspect demos.
#
# Engine code (message coding for the demo reader, collision for the map) is a vendored copy in third_party/engine,
# see third_party/README.md.

set(ENGINE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/engine)

# The collision code again under other names (engine_shim/coll_names.h): the engines that recorded the demos differ in the last bit of
# a trace (x87 registers in 32 bit x86 builds; floats where oDFe has doubles in the ioq3 based DeFRaG engines), and a recording
# only replays exactly with the collision code it was made with. host/coll.h picks per golden file.
set(SHIM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/engine_shim)
set(F32 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/engine_f32)

add_library(coll_f32 OBJECT
	${ENGINE}/cm_load.c
	${ENGINE}/cm_test.c
	${F32}/cm_trace.c
	${F32}/cm_patch.c
	${F32}/cm_polylib.c
	third_party/engine_shim/sv_world.c
)
target_include_directories(coll_f32 PRIVATE ${SHIM} ${ENGINE})
target_compile_definitions(coll_f32 PRIVATE DEDICATED COLL_SUFFIX=_f32)
target_compile_options(coll_f32 PRIVATE -O2 -ffp-contract=off "SHELL:-include ${SHIM}/coll_names.h")

set(COLL_OBJECTS $<TARGET_OBJECTS:coll_f32>)
set(COLL_DEFS "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|X86|AMD64|amd64|i[3-6]86")
	add_library(coll_x87 OBJECT
		${ENGINE}/cm_load.c
		${ENGINE}/cm_test.c
		${ENGINE}/cm_trace.c
		${ENGINE}/cm_patch.c
		${ENGINE}/cm_polylib.c
		third_party/engine_shim/sv_world.c
	)
	target_include_directories(coll_x87 PRIVATE ${SHIM} ${ENGINE})
	target_compile_definitions(coll_x87 PRIVATE DEDICATED COLL_SUFFIX=_x87)
	target_compile_options(coll_x87 PRIVATE -O2 -ffp-contract=off -mfpmath=387 "SHELL:-include ${SHIM}/coll_names.h")
	list(APPEND COLL_OBJECTS $<TARGET_OBJECTS:coll_x87>)
	set(COLL_DEFS DFG_HAVE_X87)
endif()

# golden file compression: the vendored miniz (default) or the system zlib; both read and write the same format
option(DFG_ZLIB "Use the system zlib instead of the vendored miniz" OFF)
if(DFG_ZLIB)
	if(MINGW)
		set(ZLIB_USE_STATIC_LIBS ON)
	endif()
	find_package(ZLIB)
	set(ZIP_SOURCES "")
	set(COLL_DEFS ${COLL_DEFS} DFG_USE_ZLIB)
else()
	set(ZIP_SOURCES third_party/miniz/miniz.c)
endif()

add_executable(dfgolden
	dfgolden/main.c
	dfgolden/golden.c
	dfgolden/goldmake.c
	dfgolden/goldrun.c
	dfgolden/goldmain.c
	dfgolden/demo.c
	dfgolden/steps.c
	dfgolden/gamerun.c
	dfgolden/gamesteps.c
	dfgolden/psdiff.c
	dfgolden/cfgcompare.c
	host/pmhost.c
	host/gamehost.c
	host/pmvm.c
	common/enginestubs.c
	third_party/engine_shim/sv_world.c
	${ENGINE}/msg.c
	${ENGINE}/huffman.c
	${ENGINE}/huffman_static.c
	${ENGINE}/q_shared.c
	${ENGINE}/q_math.c
	${ENGINE}/cm_load.c
	${ENGINE}/cm_test.c
	${ENGINE}/cm_trace.c
	${ENGINE}/cm_patch.c
	${ENGINE}/cm_polylib.c
	${ENGINE}/md4.c
	${ZIP_SOURCES}
	${COLL_OBJECTS}
)
# the shim's server.h has to win over any other file of that name
target_include_directories(dfgolden PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/engine_shim
	${ENGINE}
)
target_compile_definitions(dfgolden PRIVATE DEDICATED ${COLL_DEFS})
# the tests compare floats bit for bit: never let the compiler fuse operations
target_compile_options(dfgolden PRIVATE -O2 -Wall -Wno-unused-function -ffp-contract=off)
target_link_libraries(dfgolden m)
if(DFG_ZLIB)
	if(ZLIB_FOUND)
		target_link_libraries(dfgolden ZLIB::ZLIB)
	else()
		# a compiler that ships zlib in its own prefix; static, so the exe needs no dll next to it
		target_link_libraries(dfgolden -l:libz.a)
	endif()
endif()
