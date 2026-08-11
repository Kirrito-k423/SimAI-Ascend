/******************************************************************************
PROTOTYPE ONLY — throwaway evidence for the Ascend provider seam decision.

Question: can Layer dispatch an Ascend Analytical collective request through a
cost-model interface while a null pointer preserves the legacy GPU/cal_busbw
path? This file is not a calibrated HCCL model and must not ship as production.
*******************************************************************************/

#ifndef __COLLECTIVE_COST_MODEL_PROTOTYPE_HH__
#define __COLLECTIVE_COST_MODEL_PROTOTYPE_HH__

#include <cstdint>
#include <string>

namespace AstraSim {

enum class CollectiveOpPrototype {
  AllReduce,
  AllGather,
  ReduceScatter,
  AllToAll,
  Unknown
};

enum class CollectiveGroupPrototype {
  TP,
  DP,
  PP,
  EP,
  DP_EP,
  Unknown
};

struct CollectiveCostRequestPrototype {
  CollectiveCostRequestPrototype()
      : op(CollectiveOpPrototype::Unknown),
        group(CollectiveGroupPrototype::Unknown),
        rank_count(0),
        input_bytes(0),
        tp_size(0),
        ep_size(0) {}

  CollectiveOpPrototype op;
  CollectiveGroupPrototype group;
  int rank_count;
  uint64_t input_bytes;
  int tp_size;
  int ep_size;
};

struct CollectiveCostEstimatePrototype {
  CollectiveCostEstimatePrototype(
      bool supported_value,
      uint64_t duration_ns_value,
      const std::string& reason_value)
      : supported(supported_value),
        duration_ns(duration_ns_value),
        reason(reason_value) {}

  bool supported;
  uint64_t duration_ns;
  std::string reason;
};

class CollectiveCostModelPrototype {
 public:
  virtual ~CollectiveCostModelPrototype() {}
  virtual CollectiveCostEstimatePrototype Estimate(
      const CollectiveCostRequestPrototype& request) const = 0;
};

}  // namespace AstraSim

#endif  // __COLLECTIVE_COST_MODEL_PROTOTYPE_HH__
