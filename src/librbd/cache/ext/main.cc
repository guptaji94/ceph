// STL includes
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <condition_variable>
#include <atomic>

// Boost includes
#include <boost/program_options.hpp>

// Project includes
#include "VirtStat.h"
#include "VirtCache.h"
#include "Globals.h"
#include "WorkloadRead.h"
#include "RealCache.h"
#include "DetectionModule.h"
#include "SwitchModule.h"

//prototypes
void realRun(std::vector<beliefcache::ObjectID>& ticks);
void virtRun(std::vector<beliefcache::ObjectID>& ticks, beliefcache::VirtCache& v_cache,
  beliefcache::VirtCache& test_cache);
void detectionRun(std::vector<beliefcache::ObjectID>::iterator ticksStart,
  std::vector<beliefcache::ObjectID>::iterator tickEnd);

namespace po = boost::program_options;

beliefcache::MutexVector<beliefcache::Element> prefetch;
beliefcache::MutexVector<beliefcache::Element> prefetch_dummy;
std::mutex mtx;
std::mutex cache_mutex;
std::condition_variable cv;
std::condition_variable cv2;
uint64_t ACCESS_INDEX = 0;
bool ready = false;

std::mutex virtualMutex;
std::condition_variable virtualCondition;
bool virtualReady = false;
bool virtualFinished = false;

std::mutex detectionMutex;
std::condition_variable detectionCondition;
uint64_t detectionTicksToBeProcessed = 0;
std::queue<double> detectionHitrate;

bool initialAdjustment = false;

std::atomic<bool> phaseChanged;

int main(int argc, char** argv) {

  phaseChanged = false;

  // Set program options
  po::options_description desc("Allowed options");
  desc.add_options()

    (",c", po::value<uint64_t>(&beliefcache::vcacheGlobals.rcache_size)->default_value(10),
      "Size of the Real Cache")
    (",d", po::value<uint64_t>(&beliefcache::vcacheGlobals.vcache_size)->default_value(10),
      "Size of the Virtual Cache")
    (",e", po::value<uint64_t>(&beliefcache::vcacheGlobals.cc_size)->default_value(10),
      "Maximum number of Cache Candidates")
    //(",r", "Number of Unique Most Recently Accessed Elements to Track")
    (",v", po::value<uint64_t>(&beliefcache::vcacheGlobals.voter_size)->default_value(5),
      "Number of Voters")
    (",w", po::value<uint64_t>(&beliefcache::vcacheGlobals.window_size)->default_value(5),
      "Size of the Look-Ahead History Window")
    (",t", po::value<double>(&beliefcache::vcacheGlobals.thresh)->default_value(0.12),
      "Belief Threshold")
    (",i", po::value<std::string>(&Globals::WORKLOAD_FILE_PATH)->default_value("VDA80K/CompressInput10000VDA80K"),
      "File path containing the workload")
    ("train", po::value<uint64_t>(&Globals::TRAIN)->default_value(10000),
      "Number of IO accesses to be used for training")
    ("tstart", po::value<uint64_t>(&Globals::TEST_START)->default_value(1000),
      "First test IO access index in the workload")
    ("tsize", po::value<uint64_t>(&Globals::TEST_SIZE)->default_value(1000),
      "Number of IO accesses to be considered")
    ("lru,l", po::bool_switch(&Globals::LRU)->default_value(false),
      "Run real cache with LRU caching")
    ("adj,a", po::bool_switch(&initialAdjustment)->default_value(false),
      "Perform a cache parameter adjustment during initial run")
    ("mh", po::value<uint64_t>(&beliefcache::vcacheGlobals.minimum_history_to_consider)->default_value(2),
      "Number of windows before an element is used to choose cache candidates")
    ("ct", po::value<uint64_t>(&beliefcache::adaptationGlobals.cycle_ticks)->default_value(100),
      "Number of ticks between detection cycles")
    ("sc", po::value<uint64_t>(&beliefcache::adaptationGlobals.cycles_per_test)->default_value(50),
      "Number of cycles required for switch evaluation")
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
	  
  std::vector<beliefcache::ObjectID> ticks = readWorkload(Globals::WORKLOAD_FILE_PATH);
  beliefcache::VirtCache cache(prefetch);
  beliefcache::VirtCache cache2(prefetch_dummy);

  auto startTick = ticks.begin() + Globals::TEST_START;
	auto endTick = ticks.begin() + (Globals::TEST_SIZE + Globals::TEST_START);


  std::thread rRun(realRun, std::ref(ticks));
  std::thread vRun(virtRun, std::ref(ticks), std::ref(cache), std::ref(cache2));
  std::thread dRun(detectionRun, startTick, endTick);
  std::cout << "Starting simulation" << std::endl;
  rRun.join();
  vRun.join();
  dRun.join();

  return 0;
}

void realRun(std::vector<beliefcache::ObjectID>& ticks)
{
	uint64_t hit = 0;
	uint64_t misses = 0;
  double waitTime = 0.0;
	std::cout << "Starting Real Cache" << std::endl;

  uint64_t maxElement = *std::max_element(ticks.begin(), ticks.end());
	beliefcache::RealCache cache(beliefcache::vcacheGlobals.rcache_size);

	//warm up the real cache
	for (uint64_t i = 0; i < beliefcache::vcacheGlobals.rcache_size; i++)	{
		// cache.insertElementLRU(maxElement + i + 1);
	}

	std::cout << "Finished warming up real cache\n";

	auto startTime = std::chrono::high_resolution_clock::now();
	uint64_t startTick = Globals::TEST_START - Globals::TRAIN;
	if (Globals::TRAIN > Globals::TEST_START) {
		startTick = 0;
	}
	
	for (uint64_t i = startTick; i < (Globals::TEST_SIZE + Globals::TEST_START); i++) {
		// Notify virtual cache to process this tick
		{
			std::lock_guard<std::mutex> lock(virtualMutex);
			virtualReady = true;
      virtualFinished = false;
		}
		virtualCondition.notify_one();


    // Clear stats after completing the training phase
		if (i == Globals::TEST_START) {
			hit = 0;
			misses = 0;
			cache.getUsageHistogram().clear();
		}

    // If we are in the testing phase
    if (i >= Globals::TEST_START) {
      // Notify the detection module that a tick was processed
      {
        std::lock_guard<std::mutex> lock(detectionMutex);
        detectionTicksToBeProcessed++;
        detectionHitrate.push(static_cast<double>(hit) / static_cast<double>(Globals::TEST_SIZE));
      }
      detectionCondition.notify_one();
    }

		if (cache.hit(ticks[i])) {
			hit++;
		} else {
			misses++;	
		}

    if (Globals::LRU) {
      cache.insertElementLRU(ticks[i]);
    }

    auto startWait = std::chrono::high_resolution_clock::now();
    // Wait until the virtual cache is finished with the tick
    {
      std::unique_lock<std::mutex> lock(virtualMutex);
      virtualCondition.wait(lock, []{return virtualFinished;});

      auto endWait = std::chrono::high_resolution_clock::now();
      waitTime +=
        static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endWait - startWait).count())
        / 1000.0;

      // Add all of the prefetch elements to the real cache
      // Note: this is done in the reverse order they were suggested to preserve belief order
      // in LRU mode
      for (auto i = prefetch.elements.rbegin(); i != prefetch.elements.rend(); i++) {
        if (Globals::LRU) {
          cache.insertElementLRU(i->id);
        } else {
          cache.insertElementBelief(*i);
        }
      }
      prefetch.elements.clear();
      lock.unlock();
    }
	}

  auto endTime = std::chrono::high_resolution_clock::now();
  double executionTime = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

  uint64_t unusedElements = 0;
  uint64_t totalElements = 0;
  uint64_t usedElements = 0;
  int is = 0;
  cache.finalizeHistogram();
  for (auto i : cache.getUsageHistogram()) {
    totalElements += i.second;
    ++is;
    if (i.first > 0) {
      usedElements += i.second;
    } else {
      unusedElements += i.second;
    }
  }

  std::cout << "\n\n-------------------------------------------------------------------\n";
  std::cout << "Real Simulation Parameters\n";
  std::cout << "-------------------------------------------------------------------\n";
  std::cout << "Real cache size: " << beliefcache::vcacheGlobals.rcache_size << "\n";
  std::cout << "Test data start: " << Globals::TEST_START << "\n";
  std::cout << "Test data size: " << Globals::TEST_SIZE << "\n";
  std::cout << "Training size: " << Globals::TRAIN << "\n";
  std::cout << "Using LRU caching: " << Globals::LRU << "\n";

  std::unique_lock<std::mutex> lock(mtx);
  std::cout << "\n-------------------------------------------------------------------\n";
  std::cout << "Real Simulation Results\n";
  std::cout << "-------------------------------------------------------------------\n";
  std::cout << "Hitrate: " << static_cast<double>(hit) / static_cast<double>(Globals::TEST_SIZE) << "\n";
  std::cout << misses << " misses\n";
  std::cout << "Total execution time: " << executionTime << " ms\n";
  std::cout << "Average execution time: " << executionTime / Globals::TEST_SIZE << " ms\n";
  std::cout << "Average time waiting for VirtualCache: " << waitTime / Globals::TEST_SIZE << " ms\n";
  std::cout << "Insertion count: " << cache.getNumInsertions() << "\n";
  std::cout << "Average insertions: " << cache.getNumInsertions() / Globals::TEST_SIZE << "\n";
  std::cout << "Used ratio: " << static_cast<double>(unusedElements) / static_cast<double>(totalElements) << "\n";
  std::cout << "Number of total elements " << totalElements << "\n";
  std::cout << "Number of elements in histogram " << is << "\n";
  std::cout << "Number of unused elements " << unusedElements << "\n";
  std::cout << "Number of used elements " << usedElements << "\n";

  std::cout << "\n\n--------------------------------------------------------------------\n";
  std::cout << "Real Usage Histogram\n";
  std::cout << "--------------------------------------------------------------------\n";
  for (auto i : cache.getUsageHistogram()) {
    std :: cout << i.second << " elements with " << i.first << " hits\n";
  }
  
  ready = true;
  cv2.notify_one();
}

void virtRun(std::vector<beliefcache::ObjectID>& ticks, beliefcache::VirtCache &cache,
  beliefcache::VirtCache& test_cache)
{
  std::vector<beliefcache::ObjectID> access;
  access.reserve(Globals::WINDOW_SIZE);

  beliefcache::SwitchModule switchModule(
    std::vector<beliefcache::VirtCache*>({&cache, &test_cache}));

  if (initialAdjustment) {
    switchModule.startEvaluation();
  }

  std::cout << "Starting Virtual Cache" << std::endl;

  uint64_t hits = 0;
  uint64_t misses = 0;

  auto startTime = std::chrono::high_resolution_clock::now();

  uint64_t startTick = Globals::TEST_START - Globals::TRAIN;
  if (Globals::TRAIN > Globals::TEST_START) {
    startTick = 0;
  }

  for (uint64_t i = startTick; i < (Globals::TEST_SIZE + Globals::TEST_START); i++) {
		//waits until real cache has read the next tick
	  std::unique_lock<std::mutex> lock(virtualMutex);
	  virtualCondition.wait(lock, [=](){return virtualReady;});
    lock.unlock();

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

    // NOTE: Quick and dirty hack to have it connected
    // Change later.
    if (phaseChanged) {
      phaseChanged = false;
      switchModule.startEvaluation();
    }
    
    cache.updateHistory(ticks[i]);
    if (switchModule.isEvaluating()) {
      test_cache.updateHistory(ticks[i]);
      switchModule.updateHistory(ticks[i]);
    }
    

    // Notify the real cache we've finished processing the tick
    {
      std::lock_guard<std::mutex> lock(virtualMutex);
      virtualFinished = true;
      virtualReady = false;
    }
    virtualCondition.notify_one();
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  double executionTime = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

  // Flush the current cache elements to the histogram
  cache.finalizeHistogram();

  // Determine usage stats by analyzing the histogram
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

  
  std::unique_lock<std::mutex> lock(mtx);
  cv2.wait(lock, []{return ready;});
  
  std::cout << "\n\n-------------------------------------------------------------------\n";
  std::cout << "Virtual Simulation Initial Parameters\n";
  std::cout << "-------------------------------------------------------------------\n";
  std::cout << "Virtual cache size " << beliefcache::vcacheGlobals.vcache_size << "\n";
  std::cout << "Number of cache candidates " << beliefcache::vcacheGlobals.cc_size << "\n";
  std::cout << "Belief threshold " << beliefcache::vcacheGlobals.thresh << "\n";
  std::cout << "Number of voters " << beliefcache::vcacheGlobals.voter_size << "\n";
  std::cout << "Window size " << beliefcache::vcacheGlobals.window_size << "\n";
  std::cout << "Maximum matrix size " << beliefcache::vcacheGlobals.max_matrix_size << "\n";
  std::cout << "Minimum history windows " << beliefcache::vcacheGlobals.minimum_history_to_consider
    << "\n";

  auto newParams = cache.getParameters();

  std::cout << "\n\n-------------------------------------------------------------------\n";
  std::cout << "Virtual Simulation Final Parameters\n";
  std::cout << "-------------------------------------------------------------------\n";
  std::cout << "Use initial adjustment " << initialAdjustment << "\n";
  std::cout << "Number of cache candidates " << newParams.cc_size << "\n";
  std::cout << "Belief threshold " << newParams.thresh << "\n";
  std::cout << "Number of voters " << newParams.voter_size << "\n";
  std::cout << "Window size " << newParams.window_size << "\n";

  std::cout << "\n--------------------------------------------------------------------\n";
  std::cout << "Virtual Simulation Results\n";
  std::cout << "--------------------------------------------------------------------\n";
  std::cout << "Hitrate: " << static_cast<double>(hits) / static_cast<double>(Globals::TEST_SIZE) << "\n";
  std::cout << misses << " misses\n";
  std::cout << "Total execution time: " << executionTime << " ms\n";
  std::cout << "Average execution time: " << executionTime / Globals::TEST_SIZE << " ms\n";
  std::cout << "Insertion count: " << cache.getNumInsertions() << "\n";
  std::cout << "Average insertions: " << static_cast<double>(cache.getNumInsertions()) / static_cast<double>(Globals::TEST_SIZE) << "\n";
  std::cout << "Used ratio: " << static_cast<double>(usedElements) / static_cast<double>(totalElements) << "\n";

  std::cout << "\n\n--------------------------------------------------------------------\n";
  std::cout << "Virtual Usage Histogram\n";
  std::cout << "--------------------------------------------------------------------\n";
  for (auto i : cache.getUsageHistogram()) {
    std :: cout << i.second << " elements with " << i.first << " hits\n";
  }

}

void detectionRun(std::vector<beliefcache::ObjectID>::iterator ticksStart,
  std::vector<beliefcache::ObjectID>::iterator tickEnd)
{
  bool shiftDetected = false;
  auto tick = ticksStart;
  double hitrate = 0.0;

  beliefcache::DetectionModule detection(5, [&]{shiftDetected = true; std::cout << "Shift detected\n";});

  while (tick != tickEnd) {
    // Wait for the condition variable to be triggered
    std::unique_lock<std::mutex> lock(detectionMutex);
    detectionCondition.wait(lock, []{return detectionTicksToBeProcessed > 0;});

    // Get the associated hitrate for this tick
    hitrate = detectionHitrate.front();
    detectionHitrate.pop();

    // Update the tick index
    detectionTicksToBeProcessed--;
    lock.unlock();
    detection.updateHistory(*tick, hitrate);

    tick++;
  }

  std::cout << "\n\n-------------------------------------------------------------------\n";
  std::cout << "Detection Simulation Parameters\n";
  std::cout << "-------------------------------------------------------------------\n";
  if (!shiftDetected) {
    std::cout << "No input phase shift was detected\n";
  } else {
    std::cout << "Input phase shift detected\n";
  }
}
