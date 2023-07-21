//===-- TerminationTypes.h --------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TERMINATIONTYPES_H
#define KLEE_TERMINATIONTYPES_H

#include <cstdint>

#define TERMINATION_TYPES                                                      \
  TTYPE(RUNNING, 0U, "")                                                       \
  TTYPE(Exit, 1U, "")                                                          \
  MARK(NORMAL, 1U)                                                             \
  TTYPE(Interrupted, 2U, "early")                                              \
  TTYPE(MaxDepth, 3U, "early")                                                 \
  TTYPE(OutOfMemory, 4U, "early")                                              \
  TTYPE(OutOfStackMemory, 5U, "early")                                         \
  TTYPE(MaxCycles, 6U, "early")                                                \
  TTYPE(MissedAllTargets, 7U, "miss_all_targets.early")                        \
  MARK(EARLY, 7U)                                                              \
  TTYPE(Solver, 8U, "solver.err")                                              \
  MARK(SOLVERERR, 8U)                                                          \
  TTYPE(Abort, 10U, "abort.err")                                               \
  TTYPE(Assert, 11U, "assert.err")                                             \
  TTYPE(BadVectorAccess, 12U, "bad_vector_access.err")                         \
  TTYPE(Free, 13U, "free.err")                                                 \
  TTYPE(Model, 14U, "model.err")                                               \
  TTYPE(Overflow, 15U, "overflow.err")                                         \
  TTYPE(Ptr, 16U, "ptr.err")                                                   \
  TTYPE(ReadOnly, 17U, "read_only.err")                                        \
  TTYPE(ReportError, 18U, "report_error.err")                                  \
  TTYPE(UndefinedBehavior, 19U, "undefined_behavior.err")                      \
  TTYPE(InternalOutOfMemory, 20U, "out_of_memory.err")                         \
  MARK(PROGERR, 20U)                                                           \
  TTYPE(User, 23U, "user.err")                                                 \
  MARK(USERERR, 23U)                                                           \
  TTYPE(Execution, 25U, "exec.err")                                            \
  TTYPE(External, 26U, "external.err")                                         \
  MARK(EXECERR, 26U)                                                           \
  TTYPE(Replay, 27U, "")                                                       \
  TTYPE(SilentExit, 28U, "")                                                   \
  MARK(END, 28U)

///@brief Reason an ExecutionState got terminated.
enum class StateTerminationType : std::uint8_t {
#define TTYPE(N, I, S) N = (I),
#define MARK(N, I) N = (I),
  TERMINATION_TYPES
#undef TTYPE
#undef MARK
};

namespace HaltExecution {
enum Reason {
  NotHalt = 0,
  MaxTests,
  MaxInstructions,
  MaxSteppedInstructions,
  MaxTime,
  MaxCycles,
  CovCheck,
  NoMoreStates,
  ReachedTarget,
  ErrorOnWhichShouldExit,
  Interrupt,
  MaxDepth,
  MaxStackFrames,
  MaxSolverTime,
  Unspecified
};
};

#endif
