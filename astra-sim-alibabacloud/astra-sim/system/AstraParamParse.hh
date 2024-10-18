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

#ifndef __ASTRAPARAMPARSE_HH__
#define __ASTRAPARAMPARSE_HH__

#include <iostream>
#include<sstream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstdarg>
#include <vector>
#include <cstdint>
#include "Common.hh"
#define BUSBW_PATH ""
using namespace std;

enum class ModeType { NONE, ASTRA_SIM, MOCKNCCL, ANALYTICAL };

struct NetWorkParam{
  uint32_t node_num;
  uint32_t switch_num;
  uint32_t link_num;
  uint32_t trace_num;
  uint32_t nvswitch_num;
  uint32_t gpus_per_server;
  uint32_t nics_per_server;
  uint32_t nvlink_bw;
  uint32_t nic_bw;
  GPUType gpu_type;
  float tp_ar = -1.0f; 
  float tp_ag = -1.0f; 
  float tp_rs = -1.0f; 
  float tp_ata = -1.0f; 
  float dp_ar = -1.0f; 
  float dp_ag = -1.0f;
  float dp_rs = -1.0f;
  float dp_ata = -1.0f;
  float ep_ar = -1.0f;
  float ep_ag = -1.0f; 
  float ep_rs = -1.0f; 
  float ep_ata = -1.0f; 
  std::vector<int>NVswitchs;
  std::vector<std::vector<int>>all_gpus;
};

class UserParam {
private:
  static UserParam* instance;
  static std::mutex mtx;

  UserParam() {
    thread = 1;
    gpus = {};
    workload = {};
    comm_scale = 1;
    mode = ModeType::MOCKNCCL;
  }

public:
  int thread;
  std::vector<int> gpus;
  std::vector<string> workload;
  std::vector<string> res;
  int comm_scale;
  ModeType mode;
  NetWorkParam net_work_param;



  static UserParam* getInstance(){
    std::lock_guard<std::mutex> lock(mtx);
    if(instance == nullptr){
      instance = new UserParam();
    }
    return instance;
  }
void parseYaml(NetWorkParam& params, const std::string& filename) {
    std::ifstream file(BUSBW_PATH + filename);
    if (!file) {
         std::cerr << "Unable to open file: " << filename << std::endl;
         return;
     }
    std::string line;
    std::string currentSection;
    std::getline(file, line);
    while (std::getline(file, line)) {
        // Remove whitespace
        
        line.erase(0, line.find_first_not_of(' '));
        line.erase(line.find_last_not_of(' ') + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        if (line.back() == ':') {
            currentSection = line.substr(0, line.size() - 1);
        } else {
            std::istringstream ss(line);
            std::string key, valueStr;
            if (std::getline(ss, key, ':') && ss >> valueStr) {
                key.erase(key.find_last_not_of(' ') + 1);
                
                // Remove part after comma
                auto commaPos = key.find(',');
                if (commaPos != std::string::npos) {
                    key = key.substr(0, commaPos);
                }

                if (valueStr != "null") {
                    float value = std::stof(valueStr);
                    
                    if (currentSection == "TP") {
                        if (key == "allreduce") params.tp_ar = value;
                        else if (key == "allgather") params.tp_ag = value;
                        else if (key == "reducescatter") params.tp_rs = value;
                        else if (key == "alltoall") params.tp_ata = value;
                    } else if (currentSection == "DP") {
                        if (key == "allreduce") params.dp_ar = value;
                        else if (key == "allgather") params.dp_ag = value;
                        else if (key == "reducescatter") params.dp_rs = value;
                        else if (key == "alltoall") params.dp_ata = value;
                    } else if (currentSection == "EP") {
                        if (key == "allreduce") params.ep_ar = value;
                        else if (key == "allgather") params.ep_ag = value;
                        else if (key == "reducescatter") params.ep_rs = value;
                        else if (key == "alltoall") params.ep_ata = value;
                    }
                }
            }
        }
    }
}
int parse(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "-w,     --workload          Workloads, default none" << std::endl;
            std::cout << "-g,     --gpus              Number of GPUs, default 1" << std::endl;
            std::cout << "-g_p_s, --gpus-per-server   GPUs per server" << std::endl;
            std::cout << "-r,     --result            Output results path" << std::endl;
            std::cout << "-busbw, --bus-bandwidth     Bus bandwidth file" << std::endl;
            return 1;
        } else if (arg == "-w" || arg == "--workload") {
            if (++i < argc) this->workload.push_back(argv[i]);
        } else if (arg == "-g" || arg == "--gpus") {
            if (++i < argc) this->gpus.push_back(std::stoi(argv[i]));
        } else if (arg == "-r" || arg == "--result") {
            if (++i < argc) this->res.push_back(argv[i]);
        } else if (arg == "-g_p_s" || arg == "--gpus-per-server") {
            if (++i < argc) this->net_work_param.gpus_per_server = std::stoi(argv[i]);
        } else if (arg == "-busbw" || arg == "--bus-bandwidth") {
            if (++i < argc) parseYaml(this->net_work_param,argv[i]);
        }
        else {
            return 1; 
        }
    }

    if (!this->gpus.empty()) {
        this->net_work_param.nvswitch_num = this->gpus[0] / this->net_work_param.gpus_per_server;
        this->net_work_param.switch_num = 120 + this->net_work_param.gpus_per_server;
        this->net_work_param.node_num = this->net_work_param.nvswitch_num + this->net_work_param.switch_num + this->gpus[0];
    }

    return 0;
}
  ~UserParam(){}
};

#endif // __ASTRAPARAMPARSE_HH__