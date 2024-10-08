#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(klee
  main.cpp
)

set(KLEE_LIBS
  kleeCore
)

target_link_libraries(klee ${KLEE_LIBS})
target_include_directories(klee SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(klee PRIVATE ${KLEE_INCLUDE_DIRS})
target_compile_options(klee PRIVATE ${KLEE_COMPONENT_CXX_FLAGS})
target_compile_definitions(klee PRIVATE ${KLEE_COMPONENT_CXX_DEFINES})


# Copy run script
configure_file(${CMAKE_SOURCE_DIR}/scripts/kleef ${CMAKE_BINARY_DIR}/bin/kleef COPYONLY)

install(TARGETS klee RUNTIME DESTINATION bin)

# The KLEE binary depends on the runtimes
add_dependencies(klee BuildKLEERuntimes)
