/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "CollectiveCostModel.hh"

namespace AstraSim {

const char* CostedCollectiveName(CostedCollective collective) {
  switch (collective) {
    case CostedCollective::AllReduce:
      return "ALL_REDUCE";
    case CostedCollective::AllGather:
      return "ALL_GATHER";
    case CostedCollective::ReduceScatter:
      return "REDUCE_SCATTER";
    case CostedCollective::AllToAll:
      return "ALL_TO_ALL";
    case CostedCollective::AllToAllV:
      return "ALL_TO_ALL_V";
    case CostedCollective::Unsupported:
      return "UNSUPPORTED";
  }
  return "UNSUPPORTED";
}

const char* CostedGroupTypeName(CostedGroupType group_type) {
  switch (group_type) {
    case CostedGroupType::TP:
      return "TP";
    case CostedGroupType::DP:
      return "DP";
    case CostedGroupType::EP:
      return "EP";
    case CostedGroupType::DP_EP:
      return "DP_EP";
    case CostedGroupType::Unsupported:
      return "UNSUPPORTED";
  }
  return "UNSUPPORTED";
}

}  // namespace AstraSim
