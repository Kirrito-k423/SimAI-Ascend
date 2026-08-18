/* 
*Copyright (c) 2024, Alibaba Group;
*Licensed under the Apache License, Version 2.0 (the "License");
*you may not use this file except in compliance with the License.
*You may obtain a copy of the License at

*   http://www.apache.org/licenses/LICENSE-2.0

*Unless required by applicable law or agreed to in writing, software
*distributed under the License is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*See the License for the specific language governing permissions and
*limitations under the License.
*/

#include<unistd.h>
#include<string>
#include<iostream>
#include<memory>
#include<vector>

#include "astra-sim/system/Sys.hh"
#include "astra-sim/system/MockNcclLog.h"
#include "astra-sim/system/AstraComputeAPI.hh"
#include "astra-sim/system/AstraParamParse.hh"
#include "astra-sim/workload/Layer.hh"

#include "AnalyticalNetwork.h"
#include "AnaSim.h"
#include "RunContract.h"

#define RESULT_PATH "./results/"
#define WORKLOAD_PATH ""

using namespace std;

extern std::map<std::pair<std::pair<int, int>,int>, AstraSim::ncclFlowTag> receiver_pending_queue;
extern uint32_t node_num, switch_num, link_num, trace_num, nvswitch_num, gpus_per_server;
extern std::string gpu_type;
extern std::vector<int>NVswitchs;
extern std::vector<std::vector<int>>all_gpus;
extern int ngpus_per_node;
extern map<std::pair<int, std::pair<int, int>>, struct task1> expeRecvHash;
extern map<std::pair<int, std::pair<int, int>>, int> recvHash;
extern map<std::pair<int, std::pair<int, int>>, struct task1> sentHash;
extern map<std::pair<int, int>, int64_t> nodeHash;
extern int local_rank;

std::vector<string> workloads;
std::vector<std::vector<int>> physical_dims;

struct user_param {
  int thread;
  int gpus;
  string workload;
  int comm_scale;
  user_param() {
    thread = 1;
    gpus = 1;
    workload = "";
    comm_scale = 1;
  };
  ~user_param(){};
  user_param(int _thread, int _gpus, string _workload, int _comm_scale = 1)
      : thread(_thread),
        gpus(_gpus),
        workload(_workload),
        comm_scale(_comm_scale){};
};

int main(int argc,char *argv[]) {
  UserParam* param = UserParam::getInstance();
  SimAIContract::AnalyticalRunContract run_contract =
      SimAIContract::LoadAnalyticalRunContract(argc, argv);
  if (run_contract.enabled && !run_contract.accepted) {
    if (!SimAIContract::WriteAnalyticalResultManifest(run_contract, false)) {
      std::cerr << "Unable to write Result Manifest." << std::endl;
      return 4;
    }
    return run_contract.exit_code;
  }
  std::unique_ptr<SimAIContract::HcclCostModel> hccl_cost_model;
  if (run_contract.enabled) {
    const int accelerator_count = run_contract.ascend_profiled
        ? run_contract.ascend_rank_count
        : run_contract.legacy_gpu.gpu_count;
    const int accelerators_per_server = run_contract.ascend_profiled
        ? run_contract.ascend_rank_count
        : run_contract.legacy_gpu.gpus_per_server;
    param->gpus.push_back(accelerator_count);
    param->workload = run_contract.workload_path;
    param->res = "contract-" +
        run_contract.run_manifest_sha256.substr(
            run_contract.run_manifest_sha256.size() - 12) + "-";
    param->net_work_param.gpus_per_server =
        accelerators_per_server;
    param->net_work_param.nics_per_server = run_contract.ascend_profiled
        ? 1
        : run_contract.legacy_gpu.nics_per_server;
    param->net_work_param.nvlink_bw = run_contract.ascend_profiled
        ? 0.0
        : run_contract.legacy_gpu.nvlink_bandwidth_GBps;
    param->net_work_param.bw_per_nic = run_contract.ascend_profiled
        ? 0.0
        : run_contract.legacy_gpu.nic_bandwidth_GBps;
    param->net_work_param.gpu_type = run_contract.ascend_profiled
        ? GPUType::NONE
        : run_contract.legacy_gpu.gpu_type;
    param->net_work_param.nvswitch_num =
        accelerator_count / accelerators_per_server;
    param->net_work_param.switch_num =
        120 + accelerators_per_server;
    param->net_work_param.node_num =
        param->net_work_param.nvswitch_num + param->net_work_param.switch_num +
        accelerator_count;
    if (run_contract.ascend_profiled) {
      hccl_cost_model.reset(
          new SimAIContract::HcclCostModel(run_contract.hccl_cost_model));
    }
  } else if (param->parse(argc,argv)) {
    std::cerr << "-h,     --help              Help message" << std::endl;
    return -1;
  }
  param->mode = ModeType::ANALYTICAL;
  physical_dims = {param->gpus};
  // AnaInit(argc, argv);
  uint32_t using_num_gpus = 0;
  uint32_t all_gpu_num = param->gpus[0];
  for (auto &a : physical_dims) {
    int job_npus = 1;
    for (auto &dim : a) {
      job_npus *= dim;
    }
    using_num_gpus += job_npus;
  }
  std::map<int, int> node2nvswitch; //
  for(int i = 0; i < all_gpu_num; ++ i) {
    node2nvswitch[i] = all_gpu_num + i / param->net_work_param.gpus_per_server;
  }
  for(int i = all_gpu_num; i < all_gpu_num + param->net_work_param.nvswitch_num; ++ i){
    node2nvswitch[i] = i;
    param->net_work_param.NVswitchs.push_back(i);
  }

  physical_dims[0][0] += param->net_work_param.nvswitch_num;
  using_num_gpus += param->net_work_param.nvswitch_num;

  std::vector<int> queues_per_dim(physical_dims[0].size(), 1);
  int job_npus = 1;
  for (auto dim : physical_dims[0]) {
      job_npus *= dim;
    }
  
  
  AnalyticalNetWork *analytical_network = new AnalyticalNetWork(0);
  AstraSim::Sys *systems = new AstraSim::Sys(
    analytical_network,
    nullptr,
    0,
    0,
    1,
    physical_dims[0],
    queues_per_dim,
    "",
    WORKLOAD_PATH + param->workload,
    param->comm_scale,
    1,
    1,
    1,
    0,
    RESULT_PATH + param->res,
    "Analytical_test",
    true,
    false,
    param->net_work_param.gpu_type,
    param->gpus,
    param->net_work_param.NVswitchs,
    param->net_work_param.gpus_per_server,
    hccl_cost_model.get(),
    run_contract.topology_domain,
    run_contract.topology_digest,
    run_contract.enabled ? &run_contract.workload_snapshot : nullptr
  );
  if (run_contract.target_workload_ready) {
    run_contract.target_runtime_record_format =
        systems->workload->customized_layer_records
            ? "CUSTOMIZED"
            : "STANDARD";
    for (int layer = 0; layer < systems->workload->SIZE; ++layer) {
      run_contract.target_runtime_specific_parallelism.push_back(
          systems->workload->parallelism_policy_name(
              systems->workload->layers[layer]->specific_parallellism));
    }
  }
  systems->nvswitch_id = node2nvswitch[0];
  systems->num_gpus = using_num_gpus - param->net_work_param.nvswitch_num;
  

  systems->workload->fire();
  std::cout << "SimAI begin run Analytical" << std::endl;
  AnaSim::Run();
  AnaSim::Stop();
  AnaSim::Destroy();

  std::cout << "SimAI-Analytical finished." << std::endl;
  if (run_contract.enabled &&
      !SimAIContract::WriteAnalyticalResultManifest(
          run_contract, true, hccl_cost_model.get())) {
    std::cerr << "Unable to write Result Manifest." << std::endl;
    return 4;
  }
  if (hccl_cost_model != nullptr &&
      hccl_cost_model->Summary().unsupported_request) {
    return 3;
  }
  return 0;
};
