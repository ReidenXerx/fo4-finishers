cmake_minimum_required(VERSION 3.21)

# vcpkg libraries linked INTO the dll (a dynamic default made plugins import fmt.dll and friends,
# which no game folder has: Rapport on AE, 2026-09-26). Set before project().
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

project(
	Finishers
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# No build machine's folders in the dll.
if(MSVC)
	file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/" FIN_TRIM_PREFIX)
	add_compile_options("/d1trimfile:${FIN_TRIM_PREFIX}")
endif()

# One build for 1.10.163, next-gen and the Anniversary Edition: addresses resolved at run time
# through Runtime Database (f4rd-runtime.bin), as No Auto-Greet, Idle Life and VATS Bullets do.
add_subdirectory("extern/CommonLibF4RD/CommonLibF4" "CommonLibF4")

# The DLL says what it is (a version resource): virus scanners' models trust a described file more (VATS Bullets 1.0.0).
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)
add_library("${PROJECT_NAME}" SHARED "src/PCH.h" "src/main.cpp" "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_23)
target_compile_definitions(
	"${PROJECT_NAME}"
	PRIVATE
		"FIN_PROJECT_NAME=\"${PROJECT_NAME}\""
		"FIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
		"FIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}"
		"FIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}"
		"FIN_VERSION_STRING=\"${PROJECT_VERSION}\""
)
target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries("${PROJECT_NAME}" PRIVATE CommonLibF4::CommonLibF4)
target_precompile_headers("${PROJECT_NAME}" PRIVATE "src/PCH.h")
if(MSVC)
	target_compile_options("${PROJECT_NAME}" PRIVATE "/permissive-" "/Zc:preprocessor" "/utf-8")
endif()
