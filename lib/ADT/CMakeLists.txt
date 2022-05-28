# /*
#  * This source file has been created by Yummy Research Team. Copyright (c) 2022
#  */

klee_add_component(kleeADT
  TestCaseUtils.cpp
  TestCase.cpp
)

set(LLVM_COMPONENTS
  core
  executionengine
  mcjit
  native
  support
)

klee_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(kleeADT PUBLIC ${LLVM_LIBS} ${SQLITE3_LIBRARIES})
target_link_libraries(kleeADT PRIVATE
  kleeBasic
  kleeModule
  kleaverSolver
  kleaverExpr
  kleeSupport
)
