#ifndef GLOBALS_H
#define GLOBALS_H

#include <unordered_map>
#include <string>
#include <queue>
#include <cstdint>
using namespace std;

namespace predictcache {
	struct CacheParameters {
		uint64_t vcache_size = 1000;
		uint64_t cc_size = 10;
		double thresh = 0.12;
		uint64_t voter_size = 5;
		uint64_t window_size = 5;
		uint64_t max_matrix_size = 5000;
		uint64_t minimum_history_to_consider = 2;
	};

	extern CacheParameters vcacheGlobals;
}

namespace Globals {

//double HIT_TIME = 0.0002;
        extern double MISS_TIME;

// Globals (do not modify)
	extern double ARRIVAL_TIME;
	extern bool KEEP_FULL_HISTORY;
	extern string WORKLOAD_FILE_PATH;
	extern string STRESS_RESULTS_FILE;
//MAX_NUM_FILES = range(1, 5000)
	extern uint64_t TRAIN;
	extern uint64_t TEST_START;
	extern uint64_t TEST_SIZE; 
//uint64_t SIMULATION_SIZE = 0;
	extern bool DRYRUN;
	extern bool LRU;
	extern priority_queue<uint64_t> LRU_REAL_CACHE;
	extern bool METADATA;
	extern uint64_t METADATA_SAVING;
	extern bool DEBUG;
	extern bool STRESS_TEST;



 
	extern uint64_t ACCESS_INDEX;
	typedef unordered_map<uint64_t, double> belief;
	extern belief BELIEF;
//Cache PREFETCH;
	 extern uint64_t CACHE_FLAG;
	extern uint64_t CC_FLAG;

	extern uint64_t REAL_HIT_RATE;
	extern uint64_t REAL_USED_RATIO;
	extern uint64_t REAL_INSERTION_COUNT;
	extern uint64_t METADATA_FOOTPRINT;
	extern double T_REAL;
	extern double T_VIRT;

	extern uint64_t SIMULATION_SIZE;

	extern uint64_t RCACHE_SIZE;
	extern uint64_t VCACHE_SIZE;
	extern uint64_t CC_SIZE;
	extern double THRESH;
	extern uint64_t VOTER_SIZE;
	extern uint64_t WINDOW_SIZE;
	extern uint64_t MAX_MATRIX_SIZE;

	extern uint64_t MINIMUM_HISTORY_TO_CONSIDER;
} // namespace Globals

#endif //GLOBALS_H
