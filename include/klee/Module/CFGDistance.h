//===-- CFGDistance.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_CFGDISTANCE_H
#define KLEE_CFGDISTANCE_H

#include "klee/Module/KModule.h"

namespace klee {

class CFGDistance {

  using blockDistanceMap = std::map<KBlock *, std::map<KBlock *, unsigned>>;
  using blockDistanceList =
      std::map<KBlock *, std::vector<std::pair<KBlock *, unsigned>>>;

  using functionDistanceMap =
      std::map<KFunction *, std::map<KFunction *, unsigned>>;
  using functionDistanceList =
      std::map<KFunction *, std::vector<std::pair<KFunction *, unsigned>>>;

private:

  blockDistanceMap blockDistance;
  blockDistanceMap blockBackwardDistance;
  blockDistanceList blockSortedDistance;
  blockDistanceList blockSortedBackwardDistance;

  functionDistanceMap functionDistance;
  functionDistanceMap functionBackwardDistance;
  functionDistanceList functionSortedDistance;
  functionDistanceList functionSortedBackwardDistance;

private:

  void calculateDistance(KBlock *bb);
  void calculateBackwardDistance(KBlock *bb);

  void calculateDistance(KFunction *kf);
  void calculateBackwardDistance(KFunction *kf);

public:

  const std::map<KBlock *, unsigned int> &getDistance(KBlock *kb);
  const std::map<KBlock *, unsigned int> &getBackwardDistance(KBlock *kb);
  const std::vector<std::pair<KBlock *, unsigned int>> &getSortedDistance(KBlock *kb);
  const std::vector<std::pair<KBlock *, unsigned int>>& getSortedBackwardDistance(KBlock *kb);

  const std::map<KFunction *, unsigned int> &getDistance(KFunction *kf);
  const std::map<KFunction *, unsigned int> &getBackwardDistance(KFunction *kf);
  const std::vector<std::pair<KFunction *, unsigned int>> &getSortedDistance(KFunction *kf);
  const std::vector<std::pair<KFunction *, unsigned int>> &getSortedBackwardDistance(KFunction *kf);
};

} // End klee namespace


#endif /* KLEE_CFGDISTANCE_H */
