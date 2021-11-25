include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cores STATIC
  core.h core.c
  )

add_executable(macho-test
  macho-test.c
  )
target_link_libraries(macho-test PRIVATE cores)
