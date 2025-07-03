// GH100SpareSelector.cpp : This file contains the 'main' function. Program exelwtion begins and ends there.
//

#include "SkylineSpareSelector.h"

#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <array>
#include <math.h>
#include <map>
#include <chrono>
#include <numeric>

/**
 * @brief Return true if a list of tpcs meets the minimum requirement, or false if it does not
 * 
 * @param tpc_list 
 * @param min_tpc_per_gpc 
 * @return true 
 * @return false 
 */
static bool satisfiesMinCount(const std::vector<uint32_t>& tpc_list, int min_tpc_per_gpc) {
    for (auto count : tpc_list) {
        if (count != 0 && count < static_cast<uint32_t>(min_tpc_per_gpc)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Get the optimal TPC + spare config from the perspective of TPC repair.
 * 
 * @param vgpcSkyline The skyline required by the spec
 * @param aliveTPCs The available TPCs
 * @param singletonsRequired Number of singletons required by the spec
 * @param sparesRequested Number of spares allowed
 * @param spareSelection The logical TPC config with singletons and spares selected
 * @param tpc_per_gpc Number of TPCs per GPC for the chip
 * @return int output status
 */
int getOptimalTPCDownBinForSpares(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, int tpc_per_gpc, int min_tpc_per_gpc, std::vector<uint32_t> &spareSelection, std::string& msg) {

    // Sanity check, ensure the vgpcSkyline and aliveTPCs are sorted.
    if (!std::is_sorted(vgpcSkyline.begin(), vgpcSkyline.end())) { msg = "ERROR! vgpcSkyline is not sorted!"; return -1; }
    if (!std::is_sorted(aliveTPCs.begin(), aliveTPCs.end())) { msg = "ERROR! aliveTPCs is not sorted!"; return -1; }

    if (aliveTPCs.size() != vgpcSkyline.size()) {
        msg = "aliveTPCs and vgpcSkyline must be the same size!";
        return -1;
    }

    // Sanity check, ensure at least enough are alive on a per GPC basis
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        if (aliveTPCs[i] < vgpcSkyline[i]) {
            msg = "ERROR! AliveTPCs cannot meet basic skyline requirement!"; 
            return -1;
        }
    }

    // Get number of singles available
    int nSinglesAvailable = 0;
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) nSinglesAvailable += aliveTPCs[i] - vgpcSkyline[i];

    // Sanity check, ensure there are enough singles to meet singles requirement
    if (nSinglesAvailable < singletonsRequired) { 
        std::stringstream ss;
        ss << "ERROR! Insufficient singletons available to meet skyline requirement!";
        ss << " The skyline was: ";
        for (uint32_t vgpc : vgpcSkyline) {
            ss << vgpc << "/";
        }
        ss.seekp(-1, std::ios_base::end);
        ss << " alive TPCs: ";
        for (uint32_t alive_gpc : aliveTPCs) {
            ss << alive_gpc << ",";
        }
        ss << " singletons required: " << singletonsRequired;
        msg = ss.str();
        return -1;
    }

    // Initialize variables, prepare for main loop!
    std::vector<uint32_t> allAvailable;
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++)
        for (uint32_t j = 0; j < (aliveTPCs[i] - vgpcSkyline[i]); j++)
            allAvailable.push_back(i);

    const int K = std::min(singletonsRequired + sparesRequested, (int)allAvailable.size());
    const size_t N = allAvailable.size();

    std::string singlesLocationMask(K, 1); // K leading 1's
    singlesLocationMask.resize(N, 0); // N-K trailing 0's

    std::vector<uint32_t> lwrLivingTPCs;
    lwrLivingTPCs.resize(vgpcSkyline.size(), 0);

    std::vector<uint32_t> bestScores;
    bestScores.resize(K - singletonsRequired + 1, 0);

    std::map<std::vector<uint32_t>, int> configsAlreadyTested;     // A very valuable tool to avoid scoring identical configs multiple times. Can happen after sorting
    std::map<std::vector<uint32_t>, int> maxScoreConfigs;          // Effectively contains all maximal scoring configs

    do {
        // reset lwrLivingTPCs
        for (uint32_t i = 0; i < vgpcSkyline.size(); i++) lwrLivingTPCs[i] = vgpcSkyline[i];
        for (uint32_t i = 0; i < N; i++) if (singlesLocationMask[i]) lwrLivingTPCs[allAvailable[i]]++;
        sort(lwrLivingTPCs.begin(), lwrLivingTPCs.end());
        if (configsAlreadyTested.find(lwrLivingTPCs) != configsAlreadyTested.end()) continue;   // We've already tested this config. Can happen after sorting.
        configsAlreadyTested[lwrLivingTPCs] = 1;

        if (!satisfiesMinCount(lwrLivingTPCs, min_tpc_per_gpc)) {
            continue;
        }

        // Simulate and score with 1 failure, 2 failures, etc.
        for (int i = 1; i <= K - singletonsRequired; i++) {
            // We can kill up to however many spares we are allowed, but no more.
            uint32_t lwrPasses = CountPasses(vgpcSkyline, lwrLivingTPCs, i, tpc_per_gpc);
            if (bestScores[i] > lwrPasses)
            {
                // Whoops. I guess we aren't the best after all
                maxScoreConfigs.erase(lwrLivingTPCs);
                continue;
            }

            else if (lwrPasses > bestScores[i])
            {
                maxScoreConfigs.clear();    // Clear all the max score configs when we have something higher
                for (int j = i + 1; j <= K - singletonsRequired; j++) bestScores[j] = 0;
            }

            bestScores[i] = lwrPasses;

            int debugScore = 0;
            //for (int j = 1; j <= i; j++) debugScore += bestScores[j]; // for debug
            maxScoreConfigs[lwrLivingTPCs] = debugScore;
        }
        
        if (K == singletonsRequired) maxScoreConfigs[lwrLivingTPCs] = 0;   // Special case where we don't have any spares. Just pick only possible config and continue.

    } while (std::prev_permutation(singlesLocationMask.begin(), singlesLocationMask.end()));

    // If we couldn't find any valid configs, return -1
    if (maxScoreConfigs.size() == 0) {
        return -1;
    }

    // We are finished scoring all permutations!
    // At this point in time, any key in maxScoreConfigs has an identical and maximal score
    // We may wish to do some additional scoring. Such as, for example, make GPCs as equal as possible

    // An example additional screen: Select max score config, which has the largest 'smallest GPC' possible.
    int maxMinGPC = -1;
    std::vector<uint32_t> finalConfig;
    for (const auto& v : maxScoreConfigs) {
        // Remember these are sorted, so [0] element is the least populated GPC
        if (static_cast<int>(v.first[0]) > maxMinGPC) {
            maxMinGPC = v.first[0];
            finalConfig = v.first;
        }
    }
    spareSelection = finalConfig;
    if (finalConfig.size() == 0) {
        std::cout << " size was zero " << std::endl;
    }

    return 1;   // success!
}

/**
 * @brief Add spares and singletons to the GPC with the fewest singletons
 * 
 * @param vgpcSkyline The skyline required by the spec
 * @param aliveTPCs The available TPCs
 * @param singletonsRequired Number of singletons required by the spec
 * @param sparesRequested Number of spares allowed
 * @param spareSelection The logical TPC config with singletons and spares selected
 * @return int output status
 */
int pickFewestSingletons(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, std::vector<uint32_t>& spareSelection) {
    
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        spareSelection.push_back(aliveTPCs[i]);
    }

    int totalAlive = std::accumulate(aliveTPCs.begin(), aliveTPCs.end(), 0);
    int totalWanted = std::accumulate(vgpcSkyline.begin(), vgpcSkyline.end(), 0) + singletonsRequired + sparesRequested;
    int numToKill = totalAlive - totalWanted;

    for (int sparePick = 0; sparePick < numToKill; sparePick++) {
        // pick the GPC with most singletons to kill from
        auto sortFunc = [&spareSelection, &vgpcSkyline](int l, int r) -> bool {
            int l_margin = spareSelection[l] - vgpcSkyline[l];
            int r_margin = spareSelection[r] - vgpcSkyline[r];

            // if it's a tie, pick the gpc with the most tpcs to kill from
            if (l_margin == r_margin) {
                return spareSelection[l] > spareSelection[r];
            }
            return l_margin > r_margin;
        };

        std::vector<uint32_t> indexes(vgpcSkyline.size());
        std::iota(indexes.begin(), indexes.end(), 0);
        sort(indexes.begin(), indexes.end(), sortFunc);

        for (int i : indexes) {
            if (spareSelection[i] > vgpcSkyline[i]){
                spareSelection[i]--;
                break;
            }
        }
        sort(spareSelection.begin(), spareSelection.end());
    }
    return 1;
}

/**
 * @brief Add spares and singletons to the GPC with the fewest TPCs
 * 
 * @param vgpcSkyline The skyline required by the spec
 * @param aliveTPCs The available TPCs
 * @param singletonsRequired Number of singletons required by the spec
 * @param sparesRequested Number of spares allowed
 * @param spareSelection The logical TPC config with singletons and spares selected
 * @return int output status
 */
int pickFewestTPCs(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, std::vector<uint32_t>& spareSelection) {

    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        spareSelection.push_back(vgpcSkyline[i]);
    }

    // Simple algorithm. One to select TPC in most populated GPC, the other in the least populated TPC
    for (int sparePick = 0; sparePick < sparesRequested + singletonsRequired; sparePick++) {

        // pick the GPC with fewest TPCs
        for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
            if (aliveTPCs[i] > spareSelection[i]) { spareSelection[i]++; break; }
        }

        sort(spareSelection.begin(), spareSelection.end());
    }

    return 1;

}

/**
 * @brief Add spares and singletons to the GPC with the most TPCs
 * 
 * @param vgpcSkyline The skyline required by the spec
 * @param aliveTPCs The available TPCs
 * @param singletonsRequired Number of singletons required by the spec
 * @param sparesRequested Number of spares allowed
 * @param spareSelection The logical TPC config with singletons and spares selected
 * @return int output status
 */
int pickMostTPCs(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& aliveTPCs, int singletonsRequired, int sparesRequested, std::vector<uint32_t>& spareSelection) {
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        spareSelection.push_back(vgpcSkyline[i]);
    }
    

    // Simple algorithm. One to select TPC in most populated GPC, the other in the least populated TPC
    for (int sparePick = 0; sparePick < sparesRequested + singletonsRequired; sparePick++) {

        // pick the GPC with the most TPCs
        for (int i = vgpcSkyline.size() - 1; i >= 0; i--) {
            if (aliveTPCs[i] > spareSelection[i]) { spareSelection[i]++; break; }
        }

        sort(spareSelection.begin(), spareSelection.end());
    }
    return 1;
}

/**
 * @brief Return true if the lwrLivingTPCs can fit the vgpcSkyline after killing the deadIdxs
 * 
 * @param vgpcSkyline 
 * @param lwrLivingTPCs 
 * @param allAvailable 
 * @param deadIdxs a vector of flat logical tpc idxs
 * @return true 
 * @return false 
 */
bool canFit(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& lwrLivingTPCs, const std::vector<uint32_t>& allAvailable, const std::vector<uint32_t>& deadIdxs){

    std::vector<uint32_t> leftAlive(vgpcSkyline.size());
    
    // Initialize the 'leftAlive' tpcs to the current proposed living TPCs
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) leftAlive[i] = lwrLivingTPCs[i];

    // For the TPCs which are dead, remove them from leftAlive
    for (int i : deadIdxs) {
        int index_to_decrement = allAvailable[i];
        leftAlive[index_to_decrement]--;
        while (index_to_decrement != 0 && leftAlive[index_to_decrement] < leftAlive[index_to_decrement - 1]){
            std::swap(leftAlive[index_to_decrement], leftAlive[index_to_decrement - 1]);
            index_to_decrement -= 1;
        }
    }


    for (uint32_t i = 0; i < vgpcSkyline.size(); i++){
        if (leftAlive[i] < vgpcSkyline[i]) { 
            return false;
        }
    }
    return true;
}

/**
 * @brief Generate all possible N choose K combinations
 * Call fun_to_call on each one
 * 
 * @tparam T 
 * @param N 
 * @param K 
 * @param fun_to_call 
 */
template <class T>
void GenCombinationsSTL(const unsigned int N, const unsigned int K, T&& fun) {
    std::vector<uint32_t> locationMask(K, 1); // K leading 1's
    locationMask.resize(N, 0); // N-K trailing 0's
    std::vector<uint32_t> idxs;
    idxs.reserve(K);

    do {
        idxs.resize(0);
        for (int i = 0; i < N; i++) {
            if (locationMask[i]){
                idxs.push_back(i);
            }
        }
        fun(idxs);
    } while (std::prev_permutation(locationMask.begin(), locationMask.end()));
}

/**
 * @brief For printing combinations for sanity
 * 
 * @param ka 
 */
void OutputArray(const std::vector<uint32_t>& ka) {
    for (uint32_t i = 0; i < ka.size(); i++)
        std::cout << ka[i] << ",";
    std::cout << std::endl;
}

/**
 * @brief Generate all possible N choose K combinations
 * Call fun_to_call on each one
 * 
 * @tparam T 
 * @param N 
 * @param K 
 * @param fun_to_call 
 */
template <class T>
void GenCombinations(uint32_t N, uint32_t K, T&& fun_to_call) {
    if (K == 0){
        return;
    }
    std::vector<uint32_t> ka(K);
    uint32_t ki = K-1;                    
    ka[ki] = N-1;                    

    while (true) {
        uint32_t tmp = ka[ki];

        while (ki)     
            ka[--ki] = --tmp;
        fun_to_call(ka);

        while (--ka[ki] == ki) { 
            fun_to_call(ka);
            if (++ki == K) {     
                return;
            }
        }
    }
}

/**
 * @brief This function will take the required spec, and the proposed living spec, and number of failed TPCs to simulate
 * and it will return the number of passes. The more the better.
 * When this function is called, we are already ensuring we have enough required singles considering the number of failures to simulate
 * 
 * @param vgpcSkyline 
 * @param lwrLivingTPCs 
 * @param nFailures How many failures to simulate. Should be equal to the number of spares allowed or less.
 * @param tpc_per_gpc 
 * @return int 
 */
int CountPasses(const std::vector<uint32_t>& vgpcSkyline, const std::vector<uint32_t>& lwrLivingTPCs, int nFailures, int tpc_per_gpc)
{
    int passCount = 0;
    
    std::vector<uint32_t> allAvailable;       // Which GPC each TPC belongs to
    allAvailable.reserve(vgpcSkyline.size()*tpc_per_gpc);
    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        for (uint32_t j = 0; j < lwrLivingTPCs[i]; j++) allAvailable.push_back(i);
    }

    const int K = nFailures;
    const size_t N = allAvailable.size();  // Any tpc could fail!

    std::vector<uint32_t> locationMask(K, 1); // K leading 1's
    locationMask.resize(N, 0); // N-K trailing 0's


    std::vector<uint32_t> deadIdxs;
    deadIdxs.reserve(nFailures);

    auto canFitWrap = [&vgpcSkyline, &lwrLivingTPCs, &allAvailable, &passCount](const std::vector<uint32_t>& deadIdxs) {
        if (canFit(vgpcSkyline, lwrLivingTPCs, allAvailable, deadIdxs)) {
            passCount += 1;
        }
    };

    GenCombinations(static_cast<int>(N), static_cast<int>(K), canFitWrap);

    return passCount;
}

/**
 * @brief Run a few spare selection algorithms and compare them to the exhaustive algorithm
 * 
 * @param vgpcSkyline 
 * @param singletonsRequired 
 * @param sparesRequested 
 * @param tpc_per_gpc 
 */
void RunExperiments(const std::vector<uint32_t>& vgpcSkyline, int singletonsRequired, int sparesRequested, int tpc_per_gpc) {

    std::vector<uint32_t> tpcPool;    // Which GPC each extra tpc can go to
    std::vector<uint32_t> aliveTPCs(vgpcSkyline.size());

    for (uint32_t i = 0; i < vgpcSkyline.size(); i++) {
        for (int j = vgpcSkyline[i]; j < tpc_per_gpc; j++) tpcPool.push_back(i);
    }

    int totalConfigs = 0;
    int minMatched = 0;
    int maxMatched = 0;
    int fewestSingletonsMatched = 0;

    for (int32_t nExtraTPCs = singletonsRequired; nExtraTPCs <= static_cast<int>(tpcPool.size()); nExtraTPCs++) {
        std::cout << "Testing with " << nExtraTPCs << " extra TPCs\n";

        const int sparesToSelect = std::min(nExtraTPCs - singletonsRequired, sparesRequested);
        const int K = nExtraTPCs;
        const size_t N = tpcPool.size();  // Any tpc in the pool could be activated

        std::string locationMask(K, 1); // K leading 1's
        locationMask.resize(N, 0); // N-K trailing 0's
        
        auto t1 = std::chrono::high_resolution_clock::now();
        
        do {
            std::vector<uint32_t> bestSolution;

            // Set how many alive TPCs there are in this configuration
            for (uint32_t i = 0; i < vgpcSkyline.size(); i++) aliveTPCs[i] = vgpcSkyline[i];
            for (uint32_t i = 0; i < N; i++) if (locationMask[i]) aliveTPCs[tpcPool[i]]++;

            sort(aliveTPCs.begin(), aliveTPCs.end());

            // Get the best solution
            std::string msg;
            getOptimalTPCDownBinForSpares(vgpcSkyline, aliveTPCs, singletonsRequired, sparesRequested, tpc_per_gpc, 0, bestSolution, msg);

            //�pick the GPC with fewest singlegtons�, �pick the GPC with the most TPCs�, �pick the GPC with the fewest TPCs�. 

            std::vector<uint32_t> fewestSingletons;
            std::vector<uint32_t> mostTPCs;
            std::vector<uint32_t> fewestTPCs;

            pickFewestSingletons(vgpcSkyline, aliveTPCs, singletonsRequired, sparesRequested, fewestSingletons);
            pickFewestTPCs(vgpcSkyline, aliveTPCs, singletonsRequired, sparesRequested, fewestTPCs);
            pickMostTPCs(vgpcSkyline, aliveTPCs, singletonsRequired, sparesRequested, mostTPCs);

            // Compare the scores.

            int bestScore = 0;
            int mostTPCScore = 0;
            int fewestTPCScore = 0;
            int fewestSingletonScore = 0;

            for (int numFailures = 1; numFailures <= sparesToSelect; numFailures++) {
            //{
            //    int numFailures = sparesToSelect;

                bestScore += CountPasses(vgpcSkyline, bestSolution, numFailures, tpc_per_gpc);
                mostTPCScore += CountPasses(vgpcSkyline, mostTPCs, numFailures, tpc_per_gpc);
                fewestTPCScore += CountPasses(vgpcSkyline, fewestTPCs, numFailures, tpc_per_gpc);
                fewestSingletonScore += CountPasses(vgpcSkyline, fewestSingletons, numFailures, tpc_per_gpc);
            }

            if (mostTPCScore > bestScore || fewestTPCScore > bestScore || fewestSingletonScore > bestScore) std::cout << "SERIOUS WARNING!\n";
            if (mostTPCScore == bestScore) maxMatched++;
            if (fewestTPCScore == bestScore) minMatched++;
            if (fewestSingletonScore == bestScore) fewestSingletonsMatched++;
            totalConfigs++;

        } while (std::prev_permutation(locationMask.begin(), locationMask.end()));
        std::cout << "All possibilities report\n";
        std::cout << "Number of configs tested: " << totalConfigs << std::endl;
        std::cout << "Number of configs mostTPCs matched best: " << maxMatched << " : " << (float)maxMatched / (float)totalConfigs << std::endl;
        std::cout << "Number of configs fewestTPCs matched best: " << minMatched << " : " << (float)minMatched / (float)totalConfigs << std::endl;
        std::cout << "Number of configs fewestSingletons matched best: " << fewestSingletonsMatched << " : " << (float)fewestSingletonsMatched / (float)totalConfigs << std::endl;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
        std::cout << "Complete after " << ms.count() << "ms" << std::endl;
    }
}

