// PROTOTYPE ONLY: exercise the real Upstream P=4 AlltoAll flow generator.
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>

#include "astra-sim/system/MockNcclChannel.h"
#include "astra-sim/system/MockNcclGroup.h"

int main() {
  MockNccl::MockNcclGroup group(
      8, 4, 2, 4, 1, 4, 1, std::vector<int>{8, 9}, GPUType::A100);
  auto opaque = group.getFlowModels(
      MockNccl::EP,
      0,
      AstraSim::ComType::All_to_All,
      4096,
      0,
      MockNccl::State::Forward_Pass);
  auto rank_zero = std::static_pointer_cast<MockNccl::FlowModels>(opaque);
  std::set<int> unique_ids;
  std::uint64_t duplicated_rank_view_entries = 0;
  std::uint64_t unique_prev_references = 0;
  std::uint64_t network_bytes = 0;
  std::uint64_t first_flow_size = 0;
  for (const auto& cache : group.flow_models) {
    for (const auto& rank_view : cache.second) {
      duplicated_rank_view_entries += rank_view.second->size();
      for (const auto& item : *rank_view.second) {
        const auto& flow = item.second;
        if (unique_ids.insert(flow.flow_id).second) {
          unique_prev_references += flow.prev.size();
          network_bytes += flow.flow_size;
          first_flow_size = flow.flow_size;
        }
      }
    }
  }
  const bool pass =
      group.flow_models.size() == 1 && rank_zero && rank_zero->size() == 6 &&
      unique_ids.size() == 12 && duplicated_rank_view_entries == 24 &&
      unique_prev_references == 36 && first_flow_size == 1024 &&
      network_bytes == 12288;
  std::cout
      << "{\"status\":\"" << (pass ? "PASS" : "FAIL")
      << "\",\"ranks\":4"
      << ",\"unique_directed_flows\":" << unique_ids.size()
      << ",\"rank_zero_view_entries\":" << (rank_zero ? rank_zero->size() : 0)
      << ",\"duplicated_rank_view_entries\":" << duplicated_rank_view_entries
      << ",\"unique_prev_integer_references\":" << unique_prev_references
      << ",\"flow_size\":" << first_flow_size
      << ",\"network_bytes\":" << network_bytes
      << "}" << std::endl;
  return pass ? 0 : 1;
}
