# Box2D samples app

# glad for OpenGL API
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/extern/glad)

add_library(
	glad STATIC
	${GLAD_DIR}/src/glad.c
	${GLAD_DIR}/include/glad/glad.h
	${GLAD_DIR}/include/KHR/khrplatform.h
)
target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

# glfw for windowing and input
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW Docs")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW Examples")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW Tests")
set(GLFW_INSTALL OFF CACHE BOOL "GLFW Install")

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG master
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glfw)

# imgui and glfw backend for GUI
# https://gist.github.com/jeffamstutz/992723dfabac4e3ffff265eb71a24cd9
FetchContent_Populate(imgui
  URL https://github.com/ocornut/imgui/archive/docking.zip
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/build/imgui
)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/build/imgui)

add_library(imgui STATIC
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp

	${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(imgui PUBLIC glfw glad)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# jsmn for json
set(JSMN_DIR ${CMAKE_SOURCE_DIR}/extern/jsmn)

add_library(jsmn INTERFACE ${JSMN_DIR}/jsmn.h)
target_include_directories(jsmn INTERFACE ${JSMN_DIR})

add_executable(samples
	draw.cpp
	draw.h
	main.cpp
	sample.cpp
	sample.h
	settings.h
	settings.cpp
	collection/benchmark.cpp
	collection/human.cpp
	collection/human.h
	collection/sample_joints.cpp
	collection/sample_robustness.cpp
	collection/sample_bodies.cpp
	collection/sample_continuous.cpp
	collection/sample_distance.cpp
	collection/sample_dynamic_tree.cpp
	collection/sample_events.cpp
	collection/sample_hull.cpp
	collection/sample_manifold.cpp
	collection/sample_ray_cast.cpp
	collection/sample_shapes.cpp
	collection/sample_shape_cast.cpp
	collection/sample_stacking.cpp
	collection/sample_time_of_impact.cpp
)

set_target_properties(samples PROPERTIES
	CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(samples PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${JSMN_DIR})
target_link_libraries(samples PUBLIC box2d imgui glfw glad enkiTS)

# target_compile_definitions(samples PRIVATE "$<$<CONFIG:DEBUG>:SAMPLES_DEBUG>")
# message(STATUS "runtime = ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
# message(STATUS "binary = ${CMAKE_CURRENT_BINARY_DIR}")

# Copy font files, etc
add_custom_command(
	TARGET samples POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/data/
			${CMAKE_CURRENT_BINARY_DIR}/data/)

# source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${BOX2D_SAMPLES})
