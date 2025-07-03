
//-----------------------------------------------------------------------------
// A harness that generates random configs and feeds them into fslib
// example use case:
// fs_random_configs.exe --chip gh100 --sku gh100_f1 --iterations 100000 --threads 6
//-----------------------------------------------------------------------------


#include "fs_chip_hopper.h"
#include "fs_chip_ada.h"
#include "fs_chip_factory.h"
#include <map>
#include <random>
#include <limits>
#include <iostream>
#include <chrono>
#include <cxxopts.hpp>
#include <cassert>
#include <mutex>
#include <thread>
#include <functional>


namespace fslib {

typedef std::map<fslib::module_type_id, double> module_probability_map_t;
typedef std::map<fslib::Chip, module_probability_map_t> chip_probability_map_t;
typedef std::map<std::string, FUSESKUConfig::SkuConfig> sku_map_t;


/**
 * @brief Return true if there is a defect in this region.
 * 
 * @param probability 
 * @return true 
 * @return false 
 */
bool flipCoinPoisson(double probability) {
    static constexpr uint32_t seed = 0;
    static std::mt19937 gen(seed);
    std::poisson_distribution<int> distribution(probability);

    int num_defects = distribution(gen);
    return num_defects > 0;
}

/**
 * @brief Randomly floorsweep a chip based on the specified probability map
 * 
 * @param fsChip 
 * @param probabilities 
 */
void randomFloorsweep(fslib::FSChip_t& fsChip, const module_probability_map_t& probabilities) {
    // iterate over all the modules and disable based on their probabilities
    for (const auto& probability : probabilities) {
        for (FSModule_t* module_ptr : fsChip.getFlatModuleList(probability.first)) {
            module_ptr->setEnabled(!flipCoinPoisson(probability.second));
        }
    }
}

FUSESKUConfig::SkuConfig makeGH100SkuConfigF1() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 68, 68, 4, fslib::UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, fslib::UNKNOWN_ENABLE_COUNT, {5, 8, 8, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 8, 8 };
    skuconfig.fsSettings["fbp"] = { 12, 12 };
    skuconfig.fsSettings["ltc"] = { 24, 24 };
    skuconfig.fsSettings["l2slice"] = { 92, 96 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;
    return skuconfig;
}

FUSESKUConfig::SkuConfig makeGH100SkuConfigF2() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 4, fslib::UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, fslib::UNKNOWN_ENABLE_COUNT, {6, 6, 7, 7, 8, 8, 8} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };
    skuconfig.fsSettings["fbp"] = { 10, 12 };
    skuconfig.fsSettings["ltc"] = { 20, 20 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;
    return skuconfig;
}

FUSESKUConfig::SkuConfig makeGH100SkuConfigF3() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, fslib::UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, fslib::UNKNOWN_ENABLE_COUNT, {6, 6, 6, 6, 7, 7} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;
    return skuconfig;
}

FUSESKUConfig::SkuConfig makeGH100SkuConfig_hypothetical_1() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, fslib::UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, fslib::UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;
    return skuconfig;
}

FUSESKUConfig::SkuConfig makeGH100SkuConfigF1WithStricterSkyline() {
    FUSESKUConfig::SkuConfig skuconfig = makeGH100SkuConfigF1();
    skuconfig.fsSettings["tpc"] = { 68, 68, 4, fslib::UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, fslib::UNKNOWN_ENABLE_COUNT, {6, 8, 8, 8, 9, 9, 9, 9} };
    return skuconfig;
}

FUSESKUConfig::SkuConfig makeGH100SkuConfigF2WithStricterSkyline() {
    FUSESKUConfig::SkuConfig skuconfig = makeGH100SkuConfigF2();
    skuconfig.fsSettings["tpc"].skyline = { 6, 6, 6, 6, 6, 6, 6 };
    return skuconfig;
}

/**
 * @brief Create a map of skus
 * 
 * @return sku_map_t 
 */
sku_map_t createSKUs() {
    sku_map_t skus;
    skus["gh100_f1"] = makeGH100SkuConfigF1();
    skus["gh100_f2"] = makeGH100SkuConfigF2();
    skus["gh100_f3"] = makeGH100SkuConfigF3();
    skus["gh100_hypothetical_1"] = makeGH100SkuConfig_hypothetical_1();
    skus["gh100_f1_stricter_skyline"] = makeGH100SkuConfigF1WithStricterSkyline();
    skus["gh100_f2_stricter_skyline"] = makeGH100SkuConfigF2WithStricterSkyline();
    return skus;
}

/**
 * @brief Return a const ref to the map of skus
 * 
 * @return const sku_map_t& 
 */
const sku_map_t& getSKUs() {
    static sku_map_t skus = createSKUs();
    return skus;
}

/**
 * @brief Create a map of defect probabilities for each unit for each chip
 * 
 * @return chip_probability_map_t 
 */
chip_probability_map_t genChipProbabilities() {
    chip_probability_map_t chip_map;
    // From Colin Sprinkle
    chip_map[Chip::GH100][module_type_id::gpc] = 0.00631146;
    chip_map[Chip::GH100][module_type_id::tpc] = 0.0206413;
    chip_map[Chip::GH100][module_type_id::cpc] = 0.00576367;
    //chip_map[Chip::GH100][module_type_id::pes] = 0.002; // unknown
    //chip_map[Chip::GH100][module_type_id::rop] = 0.002; // unknown
    chip_map[Chip::GH100][module_type_id::fbp] = 0.0216535;
    chip_map[Chip::GH100][module_type_id::ltc] = 3.96947e-06;
    chip_map[Chip::GH100][module_type_id::l2slice] = 0.00346138;
    //chip_map[Chip::GH100][module_type_id::fbio] = 0.165015 / 6.0;


    // TODO, Get real numbers for ad10x
    for (Chip chip_name : {Chip::AD102, Chip::AD103, Chip::AD104, Chip::AD106, Chip::AD107}){
        chip_map[chip_name][module_type_id::gpc] = 0.01;
        chip_map[chip_name][module_type_id::tpc] = 0.01;
        chip_map[chip_name][module_type_id::pes] = 0.01;
        chip_map[chip_name][module_type_id::rop] = 0.01;
        chip_map[chip_name][module_type_id::fbp] = 0.01;
        chip_map[chip_name][module_type_id::ltc] = 0.01;
        chip_map[chip_name][module_type_id::l2slice] = 0.01;
        chip_map[chip_name][module_type_id::fbio] = 0.01;
    }

    return chip_map;
}

/**
 * @brief Get a string from a skyline
 * 
 * @param skyline 
 * @return std::string 
 */
std::string getSkylineStr(const std::vector<uint32_t>& skyline) {
    std::stringstream ss;
    ss << skyline[0];
    for (uint32_t i = 1; i < skyline.size(); i++) {
        ss << "/" << skyline[i];
    }
    return ss.str();
}

/**
 * @brief Get a skyline from a string
 * 
 * @param skyline_str 
 * @return std::vector<uint32_t> 
 */
std::vector<uint32_t> parseSkylineStr(const std::string& skyline_str) {

    std::vector<uint32_t> skyline;
    std::stringstream check1(skyline_str);
    std::string intermediate;

    while (std::getline(check1, intermediate, '/'))
    {
        skyline.push_back(stoi(intermediate));
    }

    return skyline;
}

/**
 * @brief Generate many random configs and print out a histogram of all the logical configs
 * 
 * @param chip_name 
 * @param iterations 
 * @return std::map<std::vector<uint32_t>, uint32_t> 
 */
std::map<std::vector<uint32_t>, uint32_t> getLogicalConfigHistogram(fslib::Chip chip_name, uint32_t iterations) {
    module_probability_map_t probabilities = genChipProbabilities()[chip_name];

    std::map<std::vector<uint32_t>, uint32_t> histo;

    for (uint32_t i = 0; i < iterations; i++) {
        if (i % 1000000 == 0) {
            std::cout << "config number: " << i << std::endl;
        }
        std::unique_ptr<FSChip_t> fsChip = createChip(chip_name);
        randomFloorsweep(*fsChip, probabilities);

        std::vector<uint32_t> logicalConfig = fsChip->getLogicalTPCCounts();

        auto it = histo.find(logicalConfig);
        if (it == histo.end()) {
            histo.insert(std::make_pair(logicalConfig, 1));
        } else {
            it->second += 1;
        }
    }

    // print the histogram
    std::cout << "now sorted by skyline: " << std::endl;
    for (const auto& kv : histo) {
        std::cout << getSkylineStr(kv.first) << ": " << kv.second << std::endl;
    }
    std::cout << "now sorted by hits: " << std::endl;

    std::vector<std::vector<uint32_t> > sorted_skylines_by_hit_count;

    for (const auto& kv : histo) {
        sorted_skylines_by_hit_count.push_back(kv.first);
    }

    auto sortFunc = [&histo](const std::vector<uint32_t>& left, const std::vector<uint32_t>& right) -> bool {
        return histo.find(left)->second > histo.find(right)->second;
    };    

    std::stable_sort(sorted_skylines_by_hit_count.begin(), sorted_skylines_by_hit_count.end(), sortFunc);

    for (const auto& vgpc_skyline : sorted_skylines_by_hit_count) {
        std::cout << getSkylineStr(vgpc_skyline) << ": " << histo.find(vgpc_skyline)->second << std::endl;
    }

    return histo;
}

/**
 * @brief Create a sorted list of reasons for not fitting the sku, from most common to least common
 * 
 * @param reasons_for_not_fitting_histogram 
 * @param reasons 
 */
void sortHistogram(const std::map<std::string, int>& reasons_for_not_fitting_histogram, std::vector<std::string>& reasons) {
    for (const auto& kv : reasons_for_not_fitting_histogram) {
        reasons.push_back(kv.first);
    }
    auto sortFunc = [&reasons_for_not_fitting_histogram](const std::string& left, const std::string& right) -> bool {
        return reasons_for_not_fitting_histogram.find(left)->second > reasons_for_not_fitting_histogram.find(right)->second;
    };
    std::stable_sort(reasons.begin(), reasons.end(), sortFunc);
}

/**
 * @brief Replace all the numbers with Xs so that the same class of reason for not fitting shows up once in the report
 * 
 * @param reason_str 
 * @return std::string 
 */
std::string getReasonStrNoNumbers(const std::string& reason_str) {
    std::string reason_string_no_numbers = reason_str;
    bool found_digit_last_char = false;
    for (uint32_t i = 0; i < reason_string_no_numbers.size(); i++) {
        if (isdigit(reason_string_no_numbers[i])) {
            reason_string_no_numbers[i] = 'X';
            if (found_digit_last_char) {
                reason_string_no_numbers.erase(i, 1);
            }
            found_digit_last_char = true;
        } else {
            found_digit_last_char = false;
        }
    }
    return reason_string_no_numbers;
}

/**
 * @brief This function generates random configs and tests that they can fit the sku
 * It can be run in multiple threads simultaneously for speedups.
 * 
 * @param chip_name 
 * @param iterations 
 * @param probabilities 
 * @param skuconfig 
 * @param iteration_number 
 * @param config_with_worst_runtime 
 * @param reasons_for_not_fitting_histogram 
 * @param max_runtime 
 * @param num_pass 
 */
void randomConfigThread(
    // input variables
    fslib::Chip chip_name,
    uint32_t iterations,
    const module_probability_map_t& probabilities,
    const fslib::FUSESKUConfig::SkuConfig& skuconfig,

    // shared variables
    uint32_t& iteration_number,
    std::unique_ptr<FSChip_t>& config_with_worst_runtime,
    std::map<std::string, int>& reasons_for_not_fitting_histogram,
    int64_t& max_runtime,
    uint32_t& num_pass

    ) {

    static std::mutex s_mutex_rng;
    static std::mutex s_mutex_iteration_number;
    static std::mutex s_mutex_bookkeeping;

    auto checkIterations = [&iteration_number, &iterations](std::mutex& s_mutex_iteration_number) -> bool {
        std::unique_lock<std::mutex> lock_iteration_number(s_mutex_iteration_number);
        iteration_number++;
        return iteration_number <= iterations;
    };

    while (checkIterations(s_mutex_bookkeeping)) {
        
        std::string msg;
        std::unique_ptr<FSChip_t> fsChip = createChip(chip_name);

        {   // make this a critical section where we randomize the config
            // This is necessary for reproducibility
            std::unique_lock<std::mutex> lock_rng(s_mutex_rng);
            randomFloorsweep(*fsChip, probabilities);
        }

        auto per_call_t1 = std::chrono::high_resolution_clock::now();
        bool canFit = fsChip->canFitSKU(skuconfig, msg);
        auto per_call_t2 = std::chrono::high_resolution_clock::now();

        {   // make this a critical section where we update all the book keeping variables
            std::unique_lock<std::mutex> lock_bookkeeping(s_mutex_bookkeeping);

            if (canFit) {
                num_pass += 1;
                msg = "";
            }
            else {
                if (reasons_for_not_fitting_histogram.find(msg) == reasons_for_not_fitting_histogram.end()) {
                    reasons_for_not_fitting_histogram.insert(std::make_pair(msg, 0));
                }
                reasons_for_not_fitting_histogram.find(msg)->second += 1;
            }

            // Enabling these prints can be helpful if you think it's stuck
            //std::cout << iteration_number << std::endl;
            //std::cout << fsChip->getString() << std::endl;
            //std::cout << msg << std::endl;

            std::chrono::milliseconds per_call_runtime = std::chrono::duration_cast<std::chrono::milliseconds>(per_call_t2 - per_call_t1);
            if (per_call_runtime.count() > max_runtime) {
                max_runtime = per_call_runtime.count();
                config_with_worst_runtime = std::move(fsChip);
            }
        }
    }
}

/**
 * @brief Run many random configs through fslib and report how many can fit the sku
 * 
 * @param chip_name 
 * @param skuconfig 
 * @param iterations 
 * @param printDetails 
 * @param replaceNumbersInMsg 
 * @param num_threads 
 */
void runRandomConfigs(fslib::Chip chip_name, const fslib::FUSESKUConfig::SkuConfig& skuconfig, uint32_t iterations, bool printDetails, bool replaceNumbersInMsg, int num_threads) {
    auto t1 = std::chrono::high_resolution_clock::now();

    module_probability_map_t probabilities = genChipProbabilities()[chip_name];

    uint32_t num_pass = 0;
    int64_t max_runtime = 0;
    std::unique_ptr<FSChip_t> config_with_worst_runtime;
    std::map<std::string, int> reasons_for_not_fitting_histogram;
    uint32_t iteration_number = 0;

    std::vector<std::thread> threads;
    auto f1 = std::bind(randomConfigThread, std::cref(chip_name), std::cref(iterations), std::cref(probabilities), std::cref(skuconfig), std::ref(iteration_number), std::ref(config_with_worst_runtime), std::ref(reasons_for_not_fitting_histogram), std::ref(max_runtime), std::ref(num_pass));
    for (int i = 1; i < num_threads; i++) {
        threads.push_back(std::thread(f1));
    }
    f1(); // Also run in the main thread
    for (auto& thread_inst : threads) {
        thread_inst.join();
    }
    

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    std::cout << "total : " << iterations << ", num passing configs : " << num_pass;
    std::cout << ", ( " << static_cast<float>(num_pass) / static_cast<float>(iterations) * 100 << "% )" << std::endl;
    std::cout << "total time: " << ms.count() << " ms on " << num_threads << " threads" << std::endl;
    std::cout << "avg time: " << (static_cast<float>(ms.count()) / static_cast<float>(iterations)) * static_cast<float>(num_threads) << " ms" << std::endl;
    std::cout << "max time: " << max_runtime << " ms" << std::endl;

    if (printDetails) {
        std::cout << "config with worst runtime: " << config_with_worst_runtime->getString() << "\n" << std::endl;

        std::cout << "These are the top reasons for why the sku didn't fit";
        if (replaceNumbersInMsg) {
            std::cout << " (numbers have been replaced with \"x\"s)";
        }
        std::cout << ":\n" << std::endl;


        std::map<std::string, int> reasons_no_numbers;
        for (const auto& reason : reasons_for_not_fitting_histogram) {
            std::string reasonWithNumbers = reason.first;
            std::string reasonNoNumbers = getReasonStrNoNumbers(reasonWithNumbers);
            auto reason_it = reasons_no_numbers.find(reasonNoNumbers);
            if (reason_it == reasons_no_numbers.end()) {
                reasons_no_numbers.insert(std::make_pair(reasonNoNumbers, reason.second));
            } else {
                reason_it->second += reason.second;
            }
        }

        std::map<std::string, int>& reaons_histo = replaceNumbersInMsg ? reasons_no_numbers : reasons_for_not_fitting_histogram;
        std::vector<std::string> sorted_reasons;
        sortHistogram(reaons_histo, sorted_reasons);
        for (const std::string& reason : sorted_reasons) {
            int num_fails_per_reason = reaons_histo.find(reason)->second;
            std::cout << num_fails_per_reason << " fails: " << reason << std::endl;
        }
    }
}
} // namespace fslib

int main (int argc, char* argv[]) {
    using namespace fslib;

    cxxopts::Options options("MyProgram", "One line description of MyProgram");

    options.add_options()
        ("c,chip", "Which chip?", cxxopts::value<std::string>())
        ("s,sku", "Which sku?", cxxopts::value<std::string>())
        ("i,iterations", "How many iterations?", cxxopts::value<uint32_t>())
        ("replaceNumber", "Replace the numbers in the histogram of reasons for not fitting the sku", cxxopts::value<bool>()->default_value("false"))
        ("d,details", "Show details about problematic configs with respect to runtime, "
            "As well as the histogram of reasons for not fitting the sku", cxxopts::value<bool>()->default_value("false"))
        ("threads", "Run in parallel on multiple threads (default is 1)", cxxopts::value<int32_t>()->default_value("1"))
        ("overrideSkyline", "Provide a skyline with x/x/x/x/x/x format. This will overwrite the predefined sku's skyline. Useful for doing a sweep of different potential skylines.", cxxopts::value<std::string>())
        ("logicalHisto", "Don't run fslib, only count the number of each logical config", cxxopts::value<bool>()->default_value("false"));

    decltype(options.parse(argc, argv)) result;
    try {
        result = options.parse(argc, argv);
    } catch (cxxopts::OptionException e){
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (!result.count("chip")) {
        std::cerr << "need to specify a chip!" << std::endl;
        std::cerr << "supported chips: ";
        for (const auto& supported_chip : fslib::getStrToChipMap()) {
            std::cerr << supported_chip.first << ", ";
        }
        std::cerr << std::endl;
        return 1;
    }

    if (!result.count("iterations")) {
        std::cerr << "need to specify number of iterations!" << std::endl;
        return 1;
    }

    std::string chip_str = result["chip"].as<std::string>().c_str();
    if (fslib::getStrToChipMap().find(chip_str) == fslib::getStrToChipMap().end()) {
        std::cerr << chip_str << " is not supported in fslib!" << std::endl;
        std::cerr << "supported chips: ";
        for (const auto& supported_chip : fslib::getStrToChipMap()) {
            std::cerr << supported_chip.first << ", ";
        }
        std::cerr << std::endl;
        return 1;
    }

    Chip chip = fslib::getStrToChipMap().find(chip_str)->second; // do something here
    uint32_t iterations = result["iterations"].as<uint32_t>();

    if (result["logicalHisto"].as<bool>()) {
        getLogicalConfigHistogram(chip, iterations);
        return 0;
    }

    if (!result.count("sku")) {
        std::cerr << "need to specify a sku!" << std::endl;
        std::cerr << "supported skus: ";
        for (const auto& sku : getSKUs()) {
            std::cerr << sku.first << ", ";
        }
        std::cerr << std::endl;
        return 1;
    }

    std::string sku_name = result["sku"].as<std::string>();
    if (getSKUs().find(sku_name) == getSKUs().end()) {
        std::cerr << sku_name << " is not defined!" << std::endl;
        std::cerr << "supported skus: ";
        for (const auto& supported_sku : getSKUs()) {
            std::cerr << supported_sku.first << ", ";
        }
        std::cerr << std::endl;
        return 1;
    }

    FUSESKUConfig::SkuConfig skuconfig = getSKUs().find(sku_name)->second;

    if (result.count("overrideSkyline")) {
        std::vector<uint32_t> skyline = parseSkylineStr(result["overrideSkyline"].as<std::string>());
        skuconfig.fsSettings.find("tpc")->second.skyline = skyline;

        std::cout << "overriding skyline with: " << getSkylineStr(skyline) << std::endl;
    }

    bool replaceNumber = result["replaceNumber"].as<bool>();
    bool printDetails = result["details"].as<bool>();
    int  num_threads = result["threads"].as<int>();
    runRandomConfigs(chip, skuconfig, iterations, printDetails, replaceNumber, num_threads);

	return 0;
}
