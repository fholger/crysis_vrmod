cmake_minimum_required(VERSION 3.15)

################################################################################

project(C1-Launcher VERSION 6 LANGUAGES CXX RC)

################################################################################

if(NOT MSVC)
	message(FATAL_ERROR "MSVC is the only supported compiler!")
endif()

if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
	# detect VS2005 compiler and use dynamically linked MSVC runtime library, otherwise use statically linked one
	if(MSVC_VERSION EQUAL 1400)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
endif()

################################################################################

math(EXPR BUILD_BITS "${CMAKE_SIZEOF_VOID_P} * 8")

message(STATUS "${PROJECT_NAME} v${PROJECT_VERSION_MAJOR} ${BUILD_BITS}-bit")

configure_file(Project.h.in ${PROJECT_BINARY_DIR}/Project.h)

################################################################################

add_library(LauncherBase STATIC
	Code/CryCommon/CryGame/IGameRef.h
	Code/CryCommon/CryGame/IGameStartup.h
	Code/CryCommon/CrySystem/IConsole.h
	Code/CryCommon/CrySystem/ICryPak.h
	Code/CryCommon/CrySystem/ILog.h
	Code/CryCommon/CrySystem/ISystem.cpp
	Code/CryCommon/CrySystem/ISystem.h
	Code/CryCommon/CrySystem/IValidator.h
	Code/Launcher/CPUInfo.cpp
	Code/Launcher/CPUInfo.h
	Code/Launcher/CryRender.h
	Code/Launcher/LauncherCommon.cpp
	Code/Launcher/LauncherCommon.h
	Code/Launcher/MemoryPatch.cpp
	Code/Launcher/MemoryPatch.h
	Code/Library/CPUID.cpp
	Code/Library/CPUID.h
	Code/Library/CrashLogger.cpp
	Code/Library/CrashLogger.h
	Code/Library/EXELoader.cpp
	Code/Library/EXELoader.h
	Code/Library/OS.cpp
	Code/Library/OS.h
	Code/Library/PathTools.cpp
	Code/Library/PathTools.h
	Code/Library/StdFile.h
	Code/Library/StringFormat.cpp
	Code/Library/StringFormat.h
	Code/Library/StringView.h
)

target_link_libraries(LauncherBase PUBLIC dbghelp)

if(BUILD_BITS EQUAL 64)
	target_compile_definitions(LauncherBase PUBLIC BUILD_64BIT)
endif()

# also include PROJECT_BINARY_DIR with configured Project.h
target_include_directories(LauncherBase PUBLIC Code ${PROJECT_BINARY_DIR})

# explicitly disable some stupid MSVC warnings
target_compile_options(LauncherBase PUBLIC /D_CRT_SECURE_NO_WARNINGS /wd4100 /wd4201 /wd4351 /wd4481 /wd4512)

# prevent modern MSVC from enabling ASLR, which breaks Crysis DLLs, and unlock memory above 2 GB
target_link_options(LauncherBase PUBLIC /DYNAMICBASE:NO /LARGEADDRESSAWARE)

################################################################################

add_library(GameLauncher STATIC
	Code/Launcher/Game/GameLauncher.cpp
	Code/Launcher/Game/GameLauncher.h
	Code/Launcher/Game/LanguageHook.cpp
	Code/Launcher/Game/LanguageHook.h
	Code/Launcher/Game/Resources.h
)

target_link_libraries(GameLauncher LauncherBase)

set(GAME_LAUNCHER_MANIFESTS
	${PROJECT_SOURCE_DIR}/Resources/Manifests/DpiAwareness.manifest
	${PROJECT_SOURCE_DIR}/Resources/Manifests/EnableVisualStyles.manifest
	${PROJECT_SOURCE_DIR}/Resources/Manifests/TrustInfo.manifest
	${PROJECT_SOURCE_DIR}/Resources/Manifests/VC80_CRT.manifest
)

################################################################################

add_executable(CrysisVR WIN32
	Code/Launcher/Game/Main.cpp
	Resources/Crysis.rc
	${GAME_LAUNCHER_MANIFESTS}
)

target_link_libraries(CrysisVR GameLauncher)

if(BUILD_BITS EQUAL 64)
  add_custom_command(TARGET CrysisVR POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CrysisVR> "${PROJECT_SOURCE_DIR}/../../../Bin64")
else()
  add_custom_command(TARGET CrysisVR POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CrysisVR> "${PROJECT_SOURCE_DIR}/../../../Bin32")
endif()

set(CURSORS
	${PROJECT_SOURCE_DIR}/Resources/CursorAmber.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorBlue.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorGreen.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorRed.cur
	${PROJECT_SOURCE_DIR}/Resources/CursorWhite.cur
)

set(CRYSIS_RESOURCES           ${CURSORS} ${PROJECT_SOURCE_DIR}/Resources/Crysis.ico)

set_source_files_properties(Resources/Crysis.rc          PROPERTIES OBJECT_DEPENDS "${CRYSIS_RESOURCES}")

################################################################################
