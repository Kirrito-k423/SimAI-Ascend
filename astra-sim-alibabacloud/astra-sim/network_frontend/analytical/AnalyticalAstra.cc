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
#include<vector>

#include "astra-sim/system/Sys.hh"
#include "astra-sim/system/MockNcclLog.h"
#include "astra-sim/system/AstraComputeAPI.hh"
#include "astra-sim/system/AstraParamParse.hh"

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
    }
    return run_contract.exit_code;
  }
  if (run_contract.enabled) {
    param->gpus.push_back(run_contract.legacy_gpu.gpu_count);
    param->workload = run_contract.workload_path;
    param->res = "contract-" +
        run_contract.run_manifest_sha256.substr(
            run_contract.run_manifest_sha256.size() - 12) + "-";
    param->net_work_param.gpus_per_server =
        run_contract.legacy_gpu.gpus_per_server;
    param->net_work_param.nics_per_server =
        run_contract.legacy_gpu.nics_per_server;
    param->net_work_param.nvlink_bw =
        run_contract.legacy_gpu.nvlink_bandwidth_GBps;
    param->net_work_param.bw_per_nic =
        run_contract.legacy_gpu.nic_bandwidth_GBps;
    if (run_contract.legacy_gpu.gpu_type == "A100") {
      param->net_work_param.gpu_type = GPUType::A100;
    } else if (run_contract.legacy_gpu.gpu_type == "A800") {
      param->net_work_param.gpu_type = GPUType::A800;
    } else if (run_contract.legacy_gpu.gpu_type == "H100") {
      param->net_work_param.gpu_type = GPUType::H100;
    } else if (run_contract.legacy_gpu.gpu_type == "H800") {
      param->net_work_param.gpu_type = GPUType::H800;
    } else {
      param->net_work_param.gpu_type = GPUType::H20;
    }
    param->net_work_param.nvswitch_num =
        run_contract.legacy_gpu.gpu_count /
        run_contract.legacy_gpu.gpus_per_server;
    param->net_work_param.switch_num =
        120 + run_contract.legacy_gpu.gpus_per_server;
    param->net_work_param.node_num =
        param->net_work_param.nvswitch_num +
        param->net_work_param.switch_num + run_contract.legacy_gpu.gpu_count;
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
    param->net_work_param.gpus_per_server
  );
  systems->nvswitch_id = node2nvswitch[0];
  systems->num_gpus = using_num_gpus - param->net_work_param.nvswitch_num;
  

  systems->workload->fire();
  std::cout << "SimAI begin run Analytical" << std::endl;
  AnaSim::Run();
  AnaSim::Stop();
  AnaSim::Destroy();

  std::cout << "SimAI-Analytical finished." << std::endl;
  if (run_contract.enabled &&
      !SimAIContract::WriteAnalyticalResultManifest(run_contract, true)) {
    std::cerr << "Unable to write Result Manifest." << std::endl;
    return 4;
  }
  return 0;
};
