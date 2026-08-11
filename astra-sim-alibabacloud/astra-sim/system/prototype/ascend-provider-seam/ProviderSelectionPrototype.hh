/******************************************************************************
PROTOTYPE ONLY — pure provider-selection state model.

Question: can explicit profile selection isolate Ascend Analytical costs from
legacy NVIDIA/NCCL and keep Simulation flow capability as a separate gate?
There is deliberately no I/O, parser, persistence, or production error type.
*******************************************************************************/

#ifndef __PROVIDER_SELECTION_PROTOTYPE_HH__
#define __PROVIDER_SELECTION_PROTOTYPE_HH__

#include <string>

namespace AstraSim {

enum class BackendPrototype { Analytical, Simulation };

struct ProviderSelectionStatePrototype {
  ProviderSelectionStatePrototype()
      : backend(BackendPrototype::Analytical),
        ascend_profile_loaded(false),
        legacy_gpu_argument(false),
        hccl_cost_model_available(true),
        hccl_flow_provider_available(false) {}

  BackendPrototype backend;
  bool ascend_profile_loaded;
  bool legacy_gpu_argument;
  bool hccl_cost_model_available;
  bool hccl_flow_provider_available;
};

struct ProviderResolutionPrototype {
  bool ready;
  bool legacy_path_untouched;
  std::string status;
  std::string accelerator_provider;
  std::string cost_model;
  std::string flow_provider;
  std::string reason;
};

inline ProviderResolutionPrototype ResolveProviderPrototype(
    const ProviderSelectionStatePrototype& state) {
  ProviderResolutionPrototype result;
  result.ready = false;
  result.legacy_path_untouched = false;
  result.status = "ERROR";
  result.accelerator_provider = "NONE";
  result.cost_model = "NONE";
  result.flow_provider = "NONE";

  if (!state.ascend_profile_loaded) {
    result.ready = true;
    result.legacy_path_untouched = true;
    result.status = "READY";
    result.accelerator_provider = "LEGACY_NVIDIA";
    result.cost_model = "LEGACY_CALBUSBW";
    result.flow_provider = state.backend == BackendPrototype::Simulation
        ? "LEGACY_MOCKNCCL"
        : "NOT_APPLICABLE";
    result.reason = "No device profile: preserve the existing CLI and code path.";
    return result;
  }

  result.accelerator_provider = "ASCEND_PROFILE";
  if (state.legacy_gpu_argument) {
    result.status = "CONFLICT";
    result.reason =
        "--device-profile and legacy --gpu_type are mutually exclusive.";
    return result;
  }
  if (!state.hccl_cost_model_available) {
    result.status = "MISSING_COST_MODEL";
    result.reason = "Ascend Analytical requires an HCCL collective cost model.";
    return result;
  }

  result.cost_model = "HCCL_COST_MODEL";
  if (state.backend == BackendPrototype::Analytical) {
    result.ready = true;
    result.status = "READY";
    result.flow_provider = "NOT_APPLICABLE";
    result.reason =
        "Ascend Analytical is isolated behind CollectiveCostModel.";
    return result;
  }

  if (!state.hccl_flow_provider_available) {
    result.status = "UNSUPPORTED_BACKEND";
    result.reason =
        "Ascend Simulation needs a CollectiveFlowProvider; never reuse MockNCCL.";
    return result;
  }

  result.ready = true;
  result.status = "READY";
  result.flow_provider = "HCCL_FLOW_PROVIDER";
  result.reason = "Future Ascend Simulation has both independent capabilities.";
  return result;
}

}  // namespace AstraSim

#endif  // __PROVIDER_SELECTION_PROTOTYPE_HH__
