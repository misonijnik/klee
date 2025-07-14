#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_library(kleeCore
  AddressSpace.cpp
        BackwardSearcher.cpp
  BidirectionalSearcher.cpp
  CallPathManager.cpp
        Composer.cpp
  CodeLocation.cpp
  Context.cpp
  CoreStats.cpp
  DistanceCalculator.cpp
  EventRecorder.cpp
  ExecutionState.cpp
  Executor.cpp
  ExecutorUtil.cpp
  ExternalDispatcher.cpp
  ImpliedValue.cpp
        IsolatedStatesInitializer.cpp
  Memory.cpp
  MemoryManager.cpp
  ObjectManager.cpp
  PForest.cpp
        ProofObligation.cpp
  MockBuilder.cpp
  PTree.cpp
  Searcher.cpp
  SeedInfo.cpp
  SeedMap.cpp
  SpecialFunctionHandler.cpp
  StatsTracker.cpp
  TargetCalculator.cpp
  TargetedExecutionReporter.cpp
  TargetedExecutionManager.cpp
  TargetManager.cpp
  TimingSolver.cpp
  UserSearcher.cpp
)

target_link_libraries(kleeCore PRIVATE
  kleeBasic
  kleeModule
  kleaverSolver
  kleaverExpr
  kleeSupport
)

llvm_config(kleeCore "${USE_LLVM_SHARED}" core executionengine mcjit native support)
target_link_libraries(kleeCore PRIVATE ${SQLite3_LIBRARIES})

target_include_directories(kleeCore SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${SQLite3_INCLUDE_DIRS})
target_include_directories(kleeCore PRIVATE ${KLEE_INCLUDE_DIRS})
target_compile_options(kleeCore PRIVATE ${KLEE_COMPONENT_CXX_FLAGS})
target_compile_definitions(kleeCore PRIVATE ${KLEE_COMPONENT_CXX_DEFINES})
