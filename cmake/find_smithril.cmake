#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
find_package(Smithril)

set(SMITHRIL_FOUND TRUE)
if (SMITHRIL_FOUND)
  set(ENABLE_SOLVER_SMITHRIL_DEFAULT ON)
else()
  set(ENABLE_SOLVER_SMITHRIL_DEFAULT OFF)
endif()

option(ENABLE_SOLVER_SMITHRIL "Enable Smithril solver support" ${ENABLE_SOLVER_SMITHRIL_DEFAULT})

if (ENABLE_SOLVER_SMITHRIL)
  message(STATUS "Smithril solver support enabled")
  if (SMITHRIL_FOUND)
    message(STATUS "Found Smithril")
    set(ENABLE_SMITHRIL 1) # For config.h

    message(STATUS "Found Smithril libraries: \"${SMITHRIL_LIBRARIES}\"")
    message(STATUS "Found Smithril include path: \"${SMITHRIL_INCLUDE_DIRS}\"")
    list(APPEND KLEE_COMPONENT_EXTRA_INCLUDE_DIRS ${SMITHRIL_INCLUDE_DIRS})
    list(APPEND KLEE_SOLVER_LIBRARIES ${SMITHRIL_LIBRARIES})
    list(APPEND KLEE_SOLVER_INCLUDE_DIRS ${SMITHRIL_INCLUDE_DIRS})
    list(APPEND KLEE_SOLVER_LIBRARY_DIRS ${SMITHRIL_LIBRARIES})

  else()
    message(FATAL_ERROR "Smithril not found.")
  endif()
else()
  message(STATUS "Smithril solver support disabled")
  set(ENABLE_SMITHRIL 0) # For config.h
endif()
