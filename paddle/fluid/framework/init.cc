/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */
#include <string.h>  // for strdup
#include <algorithm>
#include <stdexcept>
#include <string>

#include "paddle/fluid/framework/init.h"
#include "paddle/fluid/framework/operator.h"
#include "paddle/fluid/platform/device_context.h"
#include "paddle/fluid/platform/place.h"
#include "paddle/fluid/string/piece.h"

namespace paddle {
namespace framework {

std::once_flag gflags_init_flag;
std::once_flag p2p_init_flag;

void InitGflags(std::vector<std::string> &argv) {
  std::call_once(gflags_init_flag, [&]() {
    int argc = argv.size();
    char **arr = new char *[argv.size()];
    std::string line;
    for (size_t i = 0; i < argv.size(); i++) {
      arr[i] = &argv[i][0];
      line += argv[i];
      line += ' ';
    }
    google::ParseCommandLineFlags(&argc, &arr, true);
    VLOG(1) << "Init commandline: " << line;
  });
}

void InitP2P(int count) {
#ifdef PADDLE_WITH_CUDA
  std::call_once(p2p_init_flag, [&]() {
    for (int i = 0; i < count; ++i) {
      for (int j = 0; j < count; ++j) {
        if (i == j) continue;
        int can_acess = -1;
        PADDLE_ENFORCE(cudaDeviceCanAccessPeer(&can_acess, i, j),
                       "Failed to test P2P access.");
        if (can_acess != 1) {
          LOG(WARNING) << "Cannot enable P2P access from " << i << " to " << j;
        } else {
          cudaSetDevice(i);
          cudaDeviceEnablePeerAccess(j, 0);
        }
      }
    }
  });
#endif
}

void InitDevices(bool init_p2p) {
  /*Init all avaiable devices by default */

  std::vector<platform::Place> places;
  places.emplace_back(platform::CPUPlace());
  int count = 0;

#ifdef PADDLE_WITH_CUDA
  try {
    count = platform::GetCUDADeviceCount();
  } catch (const std::exception &exp) {
    LOG(WARNING) << "Compiled with WITH_GPU, but no GPU found in runtime.";
  }
#else
  LOG(WARNING)
      << "'CUDA' is not supported, Please re-compile with WITH_GPU option";
#endif

  for (int i = 0; i < count; ++i) {
    places.emplace_back(platform::CUDAPlace(i));
  }
  if (init_p2p) {
    InitP2P(count);
  }
  platform::DeviceContextPool::Init(places);
}

void InitGLOG(const std::string &prog_name) {
  // glog will not hold the ARGV[0] inside.
  // Use strdup to alloc a new string.
  google::InitGoogleLogging(strdup(prog_name.c_str()));
  google::InstallFailureSignalHandler();
}

}  // namespace framework
}  // namespace paddle
