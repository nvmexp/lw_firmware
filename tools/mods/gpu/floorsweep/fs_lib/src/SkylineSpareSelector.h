#pragma once

#include <array>
#include <vector>
#include <string>

int getOptimalTPCDownBinForSpares(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, int tpc_per_gpc, int min_tpc_per_gpc, std::vector<uint32_t>& spareSelection, std::string& msg);
int pickFewestSingletons(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, std::vector<uint32_t>& spareSelection);
int CountPasses(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& lwrLivingTPCs, int nFailures, int tpc_per_gpc);
void RunExperiments(const std::vector<uint32_t>& vgpcSkyline, int singletonsRequired, int sparesRequested, int tpc_per_gpc);