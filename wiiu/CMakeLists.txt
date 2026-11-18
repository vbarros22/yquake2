# Wii U build of Yamagi Quake II, rendering through gx2gl (OpenGL 3.3 on
# GX2) with the GL3 renderer.
#
#   cmake -S wiiu -B build-wiiu -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake \
#         -DGX2GL_SDK=/path/to/gx2gl-sdk
#   cmake --build build-wiiu
#
# Needs devkitPro's wut and wiiu-sdl2 (input and sound) and the gx2gl SDK
# (https://github.com/vbarros22/gx2gl/releases), which also contains
# CafeGLSL. The renderer and the game, normally loaded as libraries, are
# linked into the executable: each is first combined into one object
# whose symbols are made local except its entry point (GetRefAPI,
# GetGameAPI), so their copies of shared code don't clash with the
# engine's.

cmake_minimum_required(VERSION 3.13)
project(yquake2_wiiu C CXX)

set(GX2GL_SDK "" CACHE PATH "Path to the unpacked gx2gl SDK")
if(NOT EXISTS "${GX2GL_SDK}/lib/libgl33_gx2_core.a")
	message(FATAL_ERROR "Set GX2GL_SDK to the unpacked gx2gl SDK")
endif()

find_package(SDL2 REQUIRED)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fno-strict-aliasing -fwrapv -Wno-missing-braces -Wno-format-truncation -Wno-format-overflow")
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_definitions(
	YQ2OSTYPE="Wii U"
	YQ2ARCH="ppc"
	NOUNCRYPT
	IOAPI_NO_64
	)

set(Game-Source
	${SRC}/common/shared/flash.c
	${SRC}/common/shared/rand.c
	${SRC}/common/shared/shared.c
	${SRC}/game/g_ai.c
	${SRC}/game/g_chase.c
	${SRC}/game/g_cmds.c
	${SRC}/game/g_combat.c
	${SRC}/game/g_func.c
	${SRC}/game/g_items.c
	${SRC}/game/g_main.c
	${SRC}/game/g_misc.c
	${SRC}/game/g_monster.c
	${SRC}/game/g_phys.c
	${SRC}/game/g_spawn.c
	${SRC}/game/g_svcmds.c
	${SRC}/game/g_target.c
	${SRC}/game/g_trigger.c
	${SRC}/game/g_turret.c
	${SRC}/game/g_utils.c
	${SRC}/game/g_weapon.c
	${SRC}/game/monster/berserker/berserker.c
	${SRC}/game/monster/boss2/boss2.c
	${SRC}/game/monster/boss3/boss3.c
	${SRC}/game/monster/boss3/boss31.c
	${SRC}/game/monster/boss3/boss32.c
	${SRC}/game/monster/brain/brain.c
	${SRC}/game/monster/chick/chick.c
	${SRC}/game/monster/flipper/flipper.c
	${SRC}/game/monster/float/float.c
	${SRC}/game/monster/flyer/flyer.c
	${SRC}/game/monster/gladiator/gladiator.c
	${SRC}/game/monster/gunner/gunner.c
	${SRC}/game/monster/hover/hover.c
	${SRC}/game/monster/infantry/infantry.c
	${SRC}/game/monster/insane/insane.c
	${SRC}/game/monster/medic/medic.c
	${SRC}/game/monster/misc/move.c
	${SRC}/game/monster/mutant/mutant.c
	${SRC}/game/monster/parasite/parasite.c
	${SRC}/game/monster/soldier/soldier.c
	${SRC}/game/monster/supertank/supertank.c
	${SRC}/game/monster/tank/tank.c
	${SRC}/game/player/client.c
	${SRC}/game/player/hud.c
	${SRC}/game/player/trail.c
	${SRC}/game/player/view.c
	${SRC}/game/player/weapon.c
	${SRC}/game/savegame/savegame.c
	)

set(GL3-Source
	${SRC}/client/refresh/gl3/gl3_draw.c
	${SRC}/client/refresh/gl3/gl3_image.c
	${SRC}/client/refresh/gl3/gl3_light.c
	${SRC}/client/refresh/gl3/gl3_lightmap.c
	${SRC}/client/refresh/gl3/gl3_main.c
	${SRC}/client/refresh/gl3/gl3_mesh.c
	${SRC}/client/refresh/gl3/gl3_misc.c
	${SRC}/client/refresh/gl3/gl3_model.c
	${SRC}/client/refresh/gl3/gl3_wiiu.c
	${SRC}/client/refresh/gl3/gl3_surf.c
	${SRC}/client/refresh/gl3/gl3_warp.c
	${SRC}/client/refresh/gl3/gl3_shaders.c
	${SRC}/client/refresh/files/common.c
	${SRC}/client/refresh/files/models.c
	${SRC}/client/refresh/files/pcx.c
	${SRC}/client/refresh/files/stb.c
	${SRC}/client/refresh/files/surf.c
	${SRC}/client/refresh/files/wal.c
	${SRC}/client/refresh/files/pvs.c
	${SRC}/common/shared/shared.c
	${SRC}/common/md4.c
	${SRC}/client/refresh/gl3/glad/src/glad.c
	${SRC}/backends/wiiu/hunk.c
	)

set(Client-Source
	${SRC}/client/cl_cin.c
	${SRC}/client/cl_console.c
	${SRC}/client/cl_download.c
	${SRC}/client/cl_effects.c
	${SRC}/client/cl_entities.c
	${SRC}/client/cl_input.c
	${SRC}/client/cl_image.c
	${SRC}/client/cl_inventory.c
	${SRC}/client/cl_keyboard.c
	${SRC}/client/cl_lights.c
	${SRC}/client/cl_main.c
	${SRC}/client/cl_network.c
	${SRC}/client/cl_parse.c
	${SRC}/client/cl_particles.c
	${SRC}/client/cl_prediction.c
	${SRC}/client/cl_screen.c
	${SRC}/client/cl_tempentities.c
	${SRC}/client/cl_view.c
	${SRC}/client/input/gyro.c
	${SRC}/client/menu/menu.c
	${SRC}/client/menu/qmenu.c
	${SRC}/client/menu/videomenu.c
	${SRC}/client/sound/ogg.c
	${SRC}/client/sound/sdl.c
	${SRC}/client/sound/sound.c
	${SRC}/client/sound/wave.c
	${SRC}/client/vid/vid.c
	${SRC}/common/argproc.c
	${SRC}/common/clientserver.c
	${SRC}/common/collision.c
	${SRC}/common/crc.c
	${SRC}/common/cmdparser.c
	${SRC}/common/cvar.c
	${SRC}/common/filesystem.c
	${SRC}/common/glob.c
	${SRC}/common/md4.c
	${SRC}/common/movemsg.c
	${SRC}/common/frame.c
	${SRC}/common/netchan.c
	${SRC}/common/pmove.c
	${SRC}/common/szone.c
	${SRC}/common/zone.c
	${SRC}/common/shared/flash.c
	${SRC}/common/shared/rand.c
	${SRC}/common/shared/shared.c
	${SRC}/common/unzip/ioapi.c
	${SRC}/common/unzip/unzip.c
	${SRC}/common/unzip/miniz/miniz.c
	${SRC}/common/unzip/miniz/miniz_tdef.c
	${SRC}/common/unzip/miniz/miniz_tinfl.c
	${SRC}/server/sv_cmd.c
	${SRC}/server/sv_conless.c
	${SRC}/server/sv_entities.c
	${SRC}/server/sv_game.c
	${SRC}/server/sv_init.c
	${SRC}/server/sv_main.c
	${SRC}/server/sv_save.c
	${SRC}/server/sv_send.c
	${SRC}/server/sv_user.c
	${SRC}/server/sv_world.c
	${SRC}/client/input/sdl2.c
	${SRC}/client/vid/glimp_wiiu.c
	${SRC}/backends/generic/misc.c
	${SRC}/backends/wiiu/hunk.c
	${SRC}/backends/wiiu/main.c
	${SRC}/backends/wiiu/network.c
	${SRC}/backends/wiiu/system.c
	)

# Combines an object library into one relocatable object and makes every
# symbol it defines local except ENTRY.
function(yq2_module_object target entry out)
	set(combined ${CMAKE_CURRENT_BINARY_DIR}/${target}_combined.o)
	set(result ${CMAKE_CURRENT_BINARY_DIR}/${target}_module.o)
	add_custom_command(
		OUTPUT ${result}
		COMMAND ${CMAKE_LINKER} -r -o ${combined} $<TARGET_OBJECTS:${target}>
		COMMAND ${CMAKE_OBJCOPY} --keep-global-symbol=${entry} ${combined} ${result}
		DEPENDS ${target} $<TARGET_OBJECTS:${target}>
		COMMAND_EXPAND_LISTS
		VERBATIM
		COMMENT "Linking ${target} into one object with only ${entry} global")
	set(${out} ${result} PARENT_SCOPE)
endfunction()

add_library(yq2_game OBJECT ${Game-Source})
yq2_module_object(yq2_game GetGameAPI game_object)

add_library(yq2_ref_gl3 OBJECT ${GL3-Source})
target_include_directories(yq2_ref_gl3 PRIVATE "${GX2GL_SDK}/include" ${SRC}/client/refresh/gl3/glad/include)
yq2_module_object(yq2_ref_gl3 GetRefAPI ref_gl3_object)

add_executable(yquake2 ${Client-Source} ${game_object} ${ref_gl3_object})
target_include_directories(yquake2 PRIVATE "${GX2GL_SDK}/include")
target_link_libraries(yquake2 PRIVATE
	SDL2::SDL2
	"${GX2GL_SDK}/lib/libgl33_gx2_core.a"
	"${GX2GL_SDK}/lib/libcafeglsl.a"
	wut stdc++ m)

wut_create_rpx(yquake2)
wut_create_wuhb(yquake2
	NAME "Yamagi Quake II"
	SHORTNAME "Quake II"
	AUTHOR "Yamagi Quake II team, Wii U port by vbarros22")
