cmake_minimum_required(VERSION 3.14)

#**********************
# Setup XMOS toolchain
#**********************

set(XCORE_SDK_PATH "$ENV{XCORE_SDK_PATH}" CACHE FILEPATH "Absolute path to xCore SDK")

if ( XCORE_SDK_PATH STREQUAL "")
  message(FATAL_ERROR "xCore SDK path not found. User must either create an environment variable XCORE_SDK_PATH with "
                      "the fully qualified path to the xCore SDK, or manually specify the path when configuring the "
                      "CMake build via -DXCORE_SDK_PATH=/path/to/sdk")
endif()

set(ENV{XCORE_SDK_PATH} ${XCORE_SDK_PATH})

include("${XCORE_SDK_PATH}/tools/cmake_utils/xmos_toolchain.cmake")

#**********************
# Project
#**********************

# Disable in-source build.
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source build is not allowed! Please specify a build folder.\n\tex:cmake -B build")
endif()

project(l2_cache VERSION 1.0.0)

enable_language(CXX C ASM)


#**********************
# Options
#**********************

set(BUILD_TESTS TRUE CACHE BOOL "Set to build the test apps")

# set(DEFAPP "test_direct_map")
set(DEFAPP "test_two_way")

set(DEFAULT_APP ${DEFAPP} CACHE STRING "App to use when 'make flash' and 'make run' are used.")


#**********************
# Apps
#**********************
if (${BUILD_TESTS})
  add_subdirectory( tests/direct_map )
  add_subdirectory( tests/two_way )
endif()

#**********************
# Helper Targets
#**********************

add_custom_target( flash )
add_dependencies( flash "flash_${DEFAULT_APP}" )

add_custom_target( run )
add_dependencies( run "run_${DEFAULT_APP}" )
