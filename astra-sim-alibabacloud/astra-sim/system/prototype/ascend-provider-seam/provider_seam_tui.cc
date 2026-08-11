/******************************************************************************
PROTOTYPE ONLY — interactive shell around the pure provider-selection model.

This answers whether explicit AcceleratorProfile selection plus two orthogonal
capabilities (Analytical cost and Simulation flow) produces understandable,
fail-closed behavior without changing the no-profile legacy path.
*******************************************************************************/

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "astra-sim/system/prototype/ascend-provider-seam/CollectiveCostModelPrototype.hh"
#include "astra-sim/system/prototype/ascend-provider-seam/ProviderSelectionPrototype.hh"

namespace {

const char* kBold = "\x1b[1m";
const char* kDim = "\x1b[2m";
const char* kReset = "\x1b[0m";

class FakeHcclCostModelPrototype
    : public AstraSim::CollectiveCostModelPrototype {
 public:
  AstraSim::CollectiveCostEstimatePrototype Estimate(
      const AstraSim::CollectiveCostRequestPrototype& request) const {
    if (request.op == AstraSim::CollectiveOpPrototype::Unknown ||
        request.rank_count <= 0 || request.input_bytes == 0) {
      return AstraSim::CollectiveCostEstimatePrototype(
          false, 0, "PROTOTYPE_FAKE rejected an incomplete request");
    }
    return AstraSim::CollectiveCostEstimatePrototype(
        true, 424242, "PROTOTYPE_FAKE_NOT_PERFORMANCE_DATA");
  }
};

std::string Bool(bool value) { return value ? "true" : "false"; }

std::string Backend(AstraSim::BackendPrototype backend) {
  return backend == AstraSim::BackendPrototype::Analytical
      ? "ANALYTICAL"
      : "SIMULATION";
}

void PrintField(const std::string& name, const std::string& value) {
  std::cout << kBold << name << kReset << ": " << value << "\n";
}

void Render(
    const AstraSim::ProviderSelectionStatePrototype& state,
    bool clear_screen) {
  if (clear_screen) {
    std::cout << "\x1b[2J\x1b[H";
  }
  const AstraSim::ProviderResolutionPrototype resolution =
      AstraSim::ResolveProviderPrototype(state);

  std::cout << kBold << "PROTOTYPE — Ascend Provider Seam" << kReset << "\n";
  std::cout << kDim
            << "Question: isolate Ascend Analytical costs without changing "
               "legacy GPU or silently reusing MockNCCL?"
            << kReset << "\n\n";

  std::cout << kBold << "Current state" << kReset << "\n";
  PrintField("backend", Backend(state.backend));
  PrintField("ascendProfileLoaded", Bool(state.ascend_profile_loaded));
  PrintField("legacyGpuArgument", Bool(state.legacy_gpu_argument));
  PrintField("hcclCostModelAvailable", Bool(state.hccl_cost_model_available));
  PrintField("hcclFlowProviderAvailable", Bool(state.hccl_flow_provider_available));

  std::cout << "\n" << kBold << "Resolution" << kReset << "\n";
  PrintField("status", resolution.status);
  PrintField("ready", Bool(resolution.ready));
  PrintField("acceleratorProvider", resolution.accelerator_provider);
  PrintField("costModel", resolution.cost_model);
  PrintField("flowProvider", resolution.flow_provider);
  PrintField("legacyPathUntouched", Bool(resolution.legacy_path_untouched));
  PrintField("reason", resolution.reason);

  if (resolution.ready && resolution.cost_model == "HCCL_COST_MODEL") {
    FakeHcclCostModelPrototype model;
    AstraSim::CollectiveCostRequestPrototype request;
    request.op = AstraSim::CollectiveOpPrototype::AllReduce;
    request.group = AstraSim::CollectiveGroupPrototype::TP;
    request.rank_count = 8;
    request.input_bytes = 32ULL * 1024ULL * 1024ULL;
    request.tp_size = 8;
    request.ep_size = 1;
    const AstraSim::CollectiveCostEstimatePrototype estimate =
        model.Estimate(request);
    PrintField("dispatchProbe", estimate.reason);
    PrintField("fakeDurationNs", std::to_string(estimate.duration_ns));
  } else if (resolution.ready) {
    PrintField("dispatchProbe", "DELEGATE_TO_UNCHANGED_LEGACY_PATH");
  }

  std::cout << "\n" << kBold << "Actions" << kReset << "\n"
            << "[b] backend  [p] Ascend profile  [g] legacy GPU arg  "
               "[c] HCCL cost  [f] HCCL flow  [r] reset  [q] quit\n";
}

AstraSim::ProviderSelectionStatePrototype Scenario(const std::string& name) {
  AstraSim::ProviderSelectionStatePrototype state;
  if (name == "legacy-analytical") {
    return state;
  }
  if (name == "legacy-simulation") {
    state.backend = AstraSim::BackendPrototype::Simulation;
  } else if (name == "ascend-analytical") {
    state.ascend_profile_loaded = true;
  } else if (name == "ascend-simulation") {
    state.backend = AstraSim::BackendPrototype::Simulation;
    state.ascend_profile_loaded = true;
  } else if (name == "conflict") {
    state.ascend_profile_loaded = true;
    state.legacy_gpu_argument = true;
  } else if (name == "future-ascend-simulation") {
    state.backend = AstraSim::BackendPrototype::Simulation;
    state.ascend_profile_loaded = true;
    state.hccl_flow_provider_available = true;
  } else {
    std::cerr << "Unknown scenario: " << name << "\n";
    std::exit(2);
  }
  return state;
}

int Scripted(const std::string& scenario) {
  std::vector<std::string> names;
  if (scenario == "all") {
    names.push_back("legacy-analytical");
    names.push_back("legacy-simulation");
    names.push_back("ascend-analytical");
    names.push_back("ascend-simulation");
    names.push_back("conflict");
    names.push_back("future-ascend-simulation");
  } else {
    names.push_back(scenario);
  }
  for (std::size_t i = 0; i < names.size(); ++i) {
    std::cout << "\n===== scenario: " << names[i] << " =====\n";
    Render(Scenario(names[i]), false);
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--scenario") {
    return Scripted(argv[2]);
  }
  if (argc != 1) {
    std::cerr << "Usage: provider-seam-prototype [--scenario NAME|all]\n";
    return 2;
  }

  AstraSim::ProviderSelectionStatePrototype state;
  while (true) {
    Render(state, true);
    std::string action;
    if (!std::getline(std::cin, action)) {
      return 0;
    }
    if (action == "q") {
      return 0;
    } else if (action == "b") {
      state.backend = state.backend == AstraSim::BackendPrototype::Analytical
          ? AstraSim::BackendPrototype::Simulation
          : AstraSim::BackendPrototype::Analytical;
    } else if (action == "p") {
      state.ascend_profile_loaded = !state.ascend_profile_loaded;
    } else if (action == "g") {
      state.legacy_gpu_argument = !state.legacy_gpu_argument;
    } else if (action == "c") {
      state.hccl_cost_model_available = !state.hccl_cost_model_available;
    } else if (action == "f") {
      state.hccl_flow_provider_available =
          !state.hccl_flow_provider_available;
    } else if (action == "r") {
      state = AstraSim::ProviderSelectionStatePrototype();
    }
  }
}
