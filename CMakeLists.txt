cmake_minimum_required(VERSION 3.20)
project(ESPRaDeRecomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/generated")
file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS
	"${GENERATED_DIR}/program_*.c"
	"${GENERATED_DIR}/entries.c")

if(NOT GENERATED_SOURCES)
	message(FATAL_ERROR
		"No translated program in build/generated.\n"
		"Run: python tools/recompile.py rom/esprade.zip")
endif()

add_library(esprade_core STATIC
	runtime/src/m68000.c
	runtime/src/machine.c
	runtime/src/program.c
	runtime/src/video.c
	runtime/src/eeprom.c
	runtime/src/ymz280b.c
	runtime/src/sha1.c
	runtime/src/png.c
	runtime/third_party/miniz/miniz.c
	runtime/third_party/miniz/miniz_tdef.c
	runtime/third_party/miniz/miniz_tinfl.c
	runtime/third_party/miniz/miniz_zip.c
	${GENERATED_SOURCES}
)

target_include_directories(esprade_core PUBLIC
	runtime/include
	runtime/third_party/miniz
	"${GENERATED_DIR}"
)

if(MSVC)
	target_compile_options(esprade_core PRIVATE /W3 /O2)
	target_compile_definitions(esprade_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(esprade_core PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(esprade_headless ports/headless/main.c)
target_link_libraries(esprade_headless PRIVATE esprade_core)

# SDL2 front end: built when SDL2 is available (Windows: SDL2_DIR; Linux: libsdl2-dev)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
	add_executable(esprade ports/sdl2/main.c)
	target_link_libraries(esprade PRIVATE esprade_core SDL2::SDL2)
	if(WIN32)
		add_custom_command(TARGET esprade POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:esprade>)
	endif()
else()
	message(STATUS "SDL2 not found: the headless runner is built, the windowed port is not")
endif()

install(TARGETS esprade_headless RUNTIME DESTINATION bin)
if(TARGET esprade)
	install(TARGETS esprade RUNTIME DESTINATION bin)
endif()

enable_testing()
add_test(NAME headless_boot
	COMMAND esprade_headless --rom "${CMAKE_CURRENT_SOURCE_DIR}/rom/esprade.zip" --frames 600)
set_tests_properties(headless_boot PROPERTIES SKIP_RETURN_CODE 3 TIMEOUT 600)
