#pragma once

#include <cstdint>

class Stat{
public:
  uint64_t warmup_tick;
  uint64_t tick;				// indicator of the current IO access index
  uint64_t num_hit;			// total number of cache hits
  uint64_t warmup_num_hit;			// hits after cache warmup
  uint64_t num_missed;		// total number of cache misses
  uint64_t warmup_num_missed;		// misses after cache warmup
  uint64_t num_unused;	// number of unused elements
  uint64_t warmup_num_unused;
  // init function to initialize all members to zero
  Stat(uint64_t warmup_tick = 0){
    this->warmup_tick = warmup_tick;		// indicator of the IO access index corresponding to cache warmup
    this->tick = 0;				// indicator of the current IO access index
    this->num_hit = 0;			// total number of cache hits
    this->warmup_num_hit = 0;			// hits after cache warmup
    this->num_missed = 0;			// total number of cache misses
    this->warmup_num_missed = 0;		// misses after cache warmup
    this->num_unused = 0;			// number of unused elements
    this->warmup_num_unused = 0;		// number of unused elements after cache warmup
  }
  // member function to reset stats
  void reset_stat();
  void inc_hit();
  void inc_miss();
  void add_unused(uint64_t num_used);

};
