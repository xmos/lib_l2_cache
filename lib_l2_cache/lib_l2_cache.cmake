set(LIB_L2_CACHE_PATH ${CMAKE_CURRENT_LIST_DIR})

## Source files
file( GLOB_RECURSE    LIB_L2_CACHE_C_SOURCES       "${LIB_L2_CACHE_PATH}/src/*.c"   )
file( GLOB_RECURSE    LIB_L2_CACHE_CPP_SOURCES     "${LIB_L2_CACHE_PATH}/src/*.cpp" )
file( GLOB_RECURSE    LIB_L2_CACHE_ASM_SOURCES     "${LIB_L2_CACHE_PATH}/src/*.S"   )

## Compile flags for all platforms
unset(LIB_L2_CACHE_COMPILE_FLAGS)
list( APPEND  LIB_L2_CACHE_COMPILE_FLAGS     -Wno-xcore-fptrgroup)

## Set LIB_L2_CACHE_INCLUDES & LIB_L2_CACHE_SOURCES
set( LIB_L2_CACHE_INCLUDES     "${LIB_L2_CACHE_PATH}/api"         )

unset(LIB_L2_CACHE_SOURCES)
list( APPEND  LIB_L2_CACHE_SOURCES   ${LIB_L2_CACHE_C_SOURCES}    )
list( APPEND  LIB_L2_CACHE_SOURCES   ${LIB_L2_CACHE_CPP_SOURCES}  )
list( APPEND  LIB_L2_CACHE_SOURCES   ${LIB_L2_CACHE_ASM_SOURCES}  )

## Set specific file compile flags
set_source_files_properties( ${L2_CACHE_PATH}/src/l2_cache_direct_map.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable )
set_source_files_properties( ${L2_CACHE_PATH}/src/l2_cache_two_way.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable )

## cmake doesn't recognize .S files as assembly by default
set_source_files_properties( ${LIB_L2_CACHE_ASM_SOURCES} PROPERTIES LANGUAGE ASM )

foreach(COMPILE_FLAG ${LIB_L2_CACHE_COMPILE_FLAGS})
  set_source_files_properties( ${LIB_L2_CACHE_SOURCES} PROPERTIES COMPILE_FLAGS ${COMPILE_FLAG})
endforeach()
