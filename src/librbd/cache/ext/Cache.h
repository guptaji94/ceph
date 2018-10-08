#ifndef CACHE_H
#define CACHE_H
#ifndef DATA_STR
#define DATA_STR
#include <vector>
#include <unordered_map>
#include "Globals.h"
#include "Element.h"
#include <algorithm>
#endif //DATA_STR

using std::vector;
using std::unordered_map;
using namespace Globals;

namespace cache {
class Cache{
  public:
    static Cache *PREFETCH_instance;
    
    typedef vector<cache::Element> CACHEstore;		 
    CACHEstore * cache_store;
  //belief;

  // init function to initialize the cache
  Cache(uint64_t cache_size){
		// belief values kept track by the cache
    	this->cache_size = cache_size;		// size of the cache
        this->insertion_count = 0;		// insertion count	 
	this->histogram.assign(Globals::SIMULATION_SIZE, 0);
	this->cache_store = new CACHEstore();		//the cache
	this->belief = new BeliefVal();                 
  }
  static Cache *PRE_instance(uint64_t cache_size);
  void reset_stat();				//function prototypes with/without const args and output parameters 
  void hit(const uint64_t * obj, bool & found);
  void pin_element(const uint64_t * obj);
  void unpin_element(const uint64_t * obj);
  
  //destructor
  ~Cache(){
	  //destructor called and
	  //output to log that memory is released
	  
  }
   protected:

	
	uint64_t cache_size;
        uint64_t insertion_count;
	vector<uint64_t> histogram;
	typedef	unordered_map<uint64_t, double> BeliefVal;		//belief contain elem_id 
	BeliefVal * belief;						//and elem_significance
	
  

};



} //namespace cache

#endif // CACHE_H
