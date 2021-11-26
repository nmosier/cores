include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

add_library(cores STATIC
  core.h core.c
  symbols.h symbols.c
  macho.h macho.c
  util.h util.c
  bound.h bound.c
  vm.h vm.c
  )

add_executable(macho-test
  macho-test.c
  )
target_link_libraries(macho-test PRIVATE cores)
