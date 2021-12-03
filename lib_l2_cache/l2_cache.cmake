
set(L2_CACHE_PATH ${CMAKE_SOURCE_DIR}/lib_l2_cache)

## Source files
file( GLOB_RECURSE    L2_CACHE_C_SOURCES       ${L2_CACHE_PATH}/src/*.c   )
file( GLOB_RECURSE    L2_CACHE_CPP_SOURCES     ${L2_CACHE_PATH}/src/*.cpp )
file( GLOB_RECURSE    L2_CACHE_ASM_SOURCES     ${L2_CACHE_PATH}/src/*.S   )

## Compile flags
unset(L2_CACHE_COMPILE_FLAGS)
list( APPEND  L2_CACHE_COMPILE_FLAGS
              -Wno-xcore-fptrgroup
)

set_source_files_properties( ${L2_CACHE_PATH}/src/l2_cache_direct_map.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable )
set_source_files_properties( ${L2_CACHE_PATH}/src/l2_cache_two_way.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable )

# Includes
set( L2_CACHE_INCLUDES     ${L2_CACHE_PATH}/api           )

unset(L2_CACHE_SOURCES)
list( APPEND  L2_CACHE_SOURCES   ${L2_CACHE_C_SOURCES}    )
list( APPEND  L2_CACHE_SOURCES   ${L2_CACHE_CPP_SOURCES}  )
list( APPEND  L2_CACHE_SOURCES   ${L2_CACHE_ASM_SOURCES}  )

## cmake doesn't recognize .S files as assembly by default
set_source_files_properties( ${L2_CACHE_ASM_SOURCES} PROPERTIES LANGUAGE ASM )

## Apply compile flags
foreach(COMPILE_FLAG ${L2_CACHE_COMPILE_FLAGS})
  set_source_files_properties( ${L2_CACHE_SOURCES} PROPERTIES COMPILE_FLAGS ${COMPILE_FLAG})
endforeach()
