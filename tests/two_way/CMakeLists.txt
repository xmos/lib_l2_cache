
set(TEST_APP l2_cache_two_way)

set(HIL_DIR "${XCORE_SDK_PATH}/modules/hil")

#********************************
# Gather QSPI I/O sources
#********************************
set(QSPI_IO_HIL_DIR "${HIL_DIR}/lib_qspi_io")

set(QSPI_IO_HIL_FLAGS "-O2")

file(GLOB_RECURSE QSPI_IO_HIL_XC_SOURCES "${QSPI_IO_HIL_DIR}/src/*.xc")
file(GLOB_RECURSE QSPI_IO_HIL_C_SOURCES "${QSPI_IO_HIL_DIR}/src/*.c")
file(GLOB_RECURSE QSPI_IO_HIL_ASM_SOURCES "${QSPI_IO_HIL_DIR}/src/*.S")

set(QSPI_IO_HIL_SOURCES
    ${QSPI_IO_HIL_XC_SOURCES}
    ${QSPI_IO_HIL_C_SOURCES}
    ${QSPI_IO_HIL_ASM_SOURCES}
)

set_source_files_properties(${QSPI_IO_HIL_SOURCES} PROPERTIES COMPILE_FLAGS ${QSPI_IO_HIL_FLAGS})

set(QSPI_IO_HIL_INCLUDES
    "${QSPI_IO_HIL_DIR}/api"
)

#********************************
# Gather utils sources
#********************************
set(UTILS_DIR "${XCORE_SDK_PATH}/modules/utils")
file(GLOB_RECURSE UTILS_SOURCES "${UTILS_DIR}/src/*.c")

set(UTILS_INCLUDES
    "${UTILS_DIR}/api"
)

#********************************
# Gather legacy compat sources
#********************************
set(LEGACY_COMPAT_INCLUDES "${XCORE_SDK_PATH}/modules/legacy_compat")

#********************************
# Gather test sources
#********************************
include("${CMAKE_SOURCE_DIR}/lib_l2_cache/l2_cache.cmake")

#**********************
# Build flags
#**********************

add_executable(${TEST_APP})

set(FLASH_DEBUG FALSE CACHE BOOL "Set to put the flash handler in debug mode")
set(L2_CACHE_DEBUG FALSE CACHE BOOL "Set to put the L2 cache in debug mode")
set(USE_SWMEM TRUE CACHE BOOL "Set to put specified code and data in SwMem section")

set(BUILD_FLAGS
  "${CMAKE_CURRENT_SOURCE_DIR}/XCORE-AI-EXPLORER.xn"
  "-fxscope"
  "-mcmodel=large"
  "-Wno-xcore-fptrgroup"
  "-Wno-unknown-pragmas"
  "-report"
  "-g"
  "-O2"
  "-Wm,--map,memory.map"
  "-DDEBUG_PRINT_ENABLE=1"
  "-DL2_CACHE_CONFIG_FILE=\"l2_cache_config.h\""
)
target_link_options(${TEST_APP} PRIVATE ${BUILD_FLAGS} -lquadspi -w)
set_target_properties(${TEST_APP} PROPERTIES OUTPUT_NAME ${TEST_APP}.xe)

if (FLASH_DEBUG)
  list(APPEND BUILD_FLAGS "-DFLASH_DEBUG_ON=1")
endif()

if (L2_CACHE_DEBUG)
  list(APPEND BUILD_FLAGS "-DL2_CACHE_DEBUG_ON=1")
endif()

if (USE_SWMEM)
  list(APPEND BUILD_FLAGS "-DUSE_SWMEM=1")
endif()

target_compile_options(${TEST_APP} PRIVATE ${BUILD_FLAGS})

#**********************
# sources
#**********************

file( GLOB_RECURSE    SOURCES_C    "src/*.c" )
file( GLOB_RECURSE    SOURCES_CPP  "src/*.cpp" )
file( GLOB_RECURSE    SOURCES_ASM  "src/*.S" )
target_sources(${TEST_APP}
  PRIVATE ${QSPI_IO_HIL_SOURCES}
  PRIVATE ${UTILS_SOURCES}
  PRIVATE ${L2_CACHE_SOURCES}
  PRIVATE ${SOURCES_C}
  PRIVATE ${SOURCES_CPP}
  PRIVATE ${SOURCES_ASM}
)

target_include_directories(${TEST_APP}
  PRIVATE ${QSPI_IO_HIL_INCLUDES}
  PRIVATE ${UTILS_INCLUDES}
  PRIVATE ${LEGACY_COMPAT_INCLUDES}
  PRIVATE ${L2_CACHE_INCLUDES}
  PRIVATE "src"
)

# Prevent optimizing this. We want it to take up space.
set_source_files_properties(src/benchmark_data.c PROPERTIES COMPILE_FLAGS -O0)


#**********************
# install
#**********************


set(INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
make_directory(${INSTALL_DIR})

add_custom_target( install_test_two_way
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/${TEST_APP}.xe ${INSTALL_DIR}/
    DEPENDS ${TEST_APP} )


#**********************
# flash
#**********************

add_custom_target( flash_test_two_way
  COMMAND xobjdump --strip ${TEST_APP}.xe
  COMMAND xobjdump --split ${TEST_APP}.xb
  COMMAND xflash --write-all image_n0c0.swmem --target XCORE-AI-EXPLORER
  WORKING_DIRECTORY ${INSTALL_DIR}/
)
add_dependencies( flash_test_two_way ${TEST_APP} install_test_two_way )

#**********************
# run
#**********************

add_custom_target( run_test_two_way
  COMMAND xrun --xscope ${TEST_APP}.xe
  WORKING_DIRECTORY ${INSTALL_DIR}/ )

add_dependencies( run_test_two_way ${TEST_APP} install_test_two_way )
