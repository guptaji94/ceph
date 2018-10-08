// STL includes
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>

// Boost includes
#include <boost/program_options.hpp>

// Project includes
#include "VirtStat.h"
#include "VirtCache.h"
#include "Globals.h"
#include "WorkloadRead.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {

  // Set program options
  po::options_description desc("Allowed options");
  desc.add_options()
      // (",c", po::value<uint64_t>(&predictcache::vcache_globals.rcache_size)->default_value(655360),
      //   "Size of the Real Cache")
      (",d", po::value<uint64_t>(&predictcache::vcacheGlobals.vcache_size)->default_value(1000),
        "Size of the Virtual Cache")
      (",e", po::value<uint64_t>(&predictcache::vcacheGlobals.cc_size)->default_value(10),
        "Maximum number of Cache Candidates")
      //(",r", "Number of Unique Most Recently Accessed Elements to Track")
      (",v", po::value<uint64_t>(&predictcache::vcacheGlobals.voter_size)->default_value(5),
        "Number of Voters")
      (",w", po::value<uint64_t>(&predictcache::vcacheGlobals.window_size)->default_value(5),
        "Size of the Look-Ahead History Window")
      (",t", po::value<double>(&predictcache::vcacheGlobals.thresh)->default_value(0.12),
        "Belief Threshold")
      (",i", po::value<std::string>(&Globals::WORKLOAD_FILE_PATH)->default_value("VDA80K/CompressInput10000VDA80K"),
        "File path containing the workload")
      ("train", po::value<uint64_t>(&Globals::TRAIN)->default_value(10000),
        "Number of IO accesses to be used for training")
      ("tstart", po::value<uint64_t>(&Globals::TEST_START)->default_value(1000),
        "First test IO access index in the workload")
      ("tsize", po::value<uint64_t>(&Globals::TEST_SIZE)->default_value(1000),
        "Number of IO accesses to be considered")
      ("mh", po::value<uint64_t>(&predictcache::vcacheGlobals.minimum_history_to_consider)->default_value(2),
        "Number of windows before an element is used to choose cache candidates")
      // ("a", "Set the uniform IO request arrival time (in seconds) in the Real Cache, for the given closed loop workload")
      // ("dryrun", "Run only the Virtual Cache. Useful for rate matching")
      // ("lru", "Use 'Least Recently Used' replacement policy in the Real Cache")
      // ("m,meta", "Calculate Metadata Footprint of the Virtual Cache")
      // ("debug", "Enable Debug mode")
      // ("stress", "Enable Stress Test mode (to be used with the Stress Test Framework). This mode will enable metadata calculation by default!")
      // ("o", "Name of file to write the output in Stress Test mode (default = 'stressResults.csv')")
      ("help,h", "Bring up this HELP Menu");

  po::variables_map optionsMap;
  po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), optionsMap);
  po::notify(optionsMap);

  if (optionsMap.count("help")) {
    std::cout << "Runs a simulation of the BeliefCache algorithm\n" << desc << "\n";
    return 0;
  }

  std::vector<predictcache::ObjectID> ticks = readWorkload(Globals::WORKLOAD_FILE_PATH);

  predictcache::VirtCache cache(Globals::VCACHE_SIZE);

  uint64_t hits = 0;
  uint64_t misses = 0;

  auto startTime = std::chrono::high_resolution_clock::now();

  uint64_t startTick = Globals::TEST_START - Globals::TRAIN;
  if (Globals::TRAIN > Globals::TEST_START) {
    startTick = 0;
  }

  for (uint64_t i = startTick; i < (Globals::TEST_SIZE + Globals::TEST_START); i++) {
    // Reset stats after training
    if (i == Globals::TEST_START) {
      hits = 0;
      misses = 0;
      cache.getUsageHistogram().clear();
    }
    
    if (cache.hit(ticks[i])) {
      hits++;
    } else {
      misses++;
    }

    cache.updateHistory(ticks[i]);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  double executionTime = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

  cache.finalizeHistogram();

  // Needs proper formatting
  uint64_t unusedElements = 0;
  uint64_t totalElements = 0;
  uint64_t usedElements = 0;
  for (auto i : cache.getUsageHistogram()) {
    totalElements += i.second;

    if (i.first > 0) {
      usedElements += i.second;
    } else {
      unusedElements += i.second;
    }
  }

  std::cout << "\n\n-------------------------------------------------------------------\n";
  std::cout << "Simulation Parameters\n";
  std::cout << "-------------------------------------------------------------------\n";
  std::cout << "Virtual cache size " << predictcache::vcacheGlobals.vcache_size << "\n";
  std::cout << "Number of cache candidates " << predictcache::vcacheGlobals.cc_size << "\n";
  std::cout << "Belief threshold " << predictcache::vcacheGlobals.thresh << "\n";
  std::cout << "Number of voters " << predictcache::vcacheGlobals.voter_size << "\n";
  std::cout << "Window size " << predictcache::vcacheGlobals.window_size << "\n";
  std::cout << "Maximum matrix size " << predictcache::vcacheGlobals.max_matrix_size << "\n";
  std::cout << "Minimum history windows " << predictcache::vcacheGlobals.minimum_history_to_consider
    << "\n";

  std::cout << "\n--------------------------------------------------------------------\n";
  std::cout << "Simulation Results\n";
  std::cout << "--------------------------------------------------------------------\n";
  std::cout << "Hitrate: " << static_cast<double>(hits) / static_cast<double>(Globals::TEST_SIZE) << "\n";
  std::cout << misses << " misses\n";
  std::cout << "Total execution time: " << executionTime << " ms\n";
  std::cout << "Average execution time: " << executionTime / Globals::TEST_SIZE << " ms\n";
  std::cout << "Insertion count: " << cache.getNumInsertions() << "\n";
  std::cout << "Average insertions: " << static_cast<double>(cache.getNumInsertions()) / static_cast<double>(Globals::TEST_SIZE) << "\n";
  std::cout << "Used ratio: " << static_cast<double>(usedElements) / static_cast<double>(totalElements) << "\n";

  std::cout << "\n\n--------------------------------------------------------------------\n";
  std::cout << "Usage Histogram\n";
  std::cout << "--------------------------------------------------------------------\n";
  for (auto i : cache.getUsageHistogram()) {
    std :: cout << i.second << " elements with " << i.first << " hits\n";
  }

  return 0;
}
