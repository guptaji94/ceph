#include "Globals.h"

namespace predictcache {
    CacheParameters vcacheGlobals;
}

namespace Globals {

double MISS_TIME = 0.0003;

// Globals (do not modify)
double ARRIVAL_TIME = 0.00001;
bool KEEP_FULL_HISTORY = true; // avoid recency due to time-space tradeoff;
string WORKLOAD_FILE_PATH = "VDA80K/CompressInput10000VDA80K";
string STRESS_RESULTS_FILE = "stressResults.csv";
//MAX_NUM_FILES = range(1, 5000)
uint64_t TRAIN = 10000;
uint64_t TEST_START = 10000;
uint64_t TEST_SIZE = 100000;
//uint64_t SIMULATION_SIZE = 0;
bool DRYRUN = false;
bool LRU = false;
bool METADATA = false;
uint64_t METADATA_SAVING = 0.0;
bool DEBUG = false;
bool STRESS_TEST = false;



 
uint64_t ACCESS_INDEX = 0;
//Cache PREFETCH;
uint64_t CACHE_FLAG = 0;
uint64_t CC_FLAG = 1;

uint64_t REAL_HIT_RATE = 0;
uint64_t REAL_USED_RATIO = 0;
uint64_t REAL_INSERTION_COUNT = 0;
uint64_t METADATA_FOOTPRINT = 0;
double T_REAL = 0.0;
double T_VIRT = 0.0;

uint64_t SIMULATION_SIZE = 0;

uint64_t RCACHE_SIZE = 655360;
uint64_t VCACHE_SIZE = 1000;
uint64_t CC_SIZE = 10;
double THRESH = 0.12;
uint64_t VOTER_SIZE = 5;
uint64_t WINDOW_SIZE = 5;
uint64_t MAX_MATRIX_SIZE = 5000;
uint64_t MINIMUM_HISTORY_TO_CONSIDER = 2;
	
}//namespace Globals
