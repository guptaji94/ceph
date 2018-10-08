#include "Cache.h"

namespace cache {

	static Cache * PRE_instance(uint64_t cache_size){
		if (!cache::Cache::PREFETCH_instance)
			cache::Cache::PREFETCH_instance = new cache::Cache(cache_size);
		return cache::Cache::PREFETCH_instance;
	}

	void Cache::reset_stat() {

	insertion_count = 0;
	histogram.assign(Globals::SIMULATION_SIZE, 0);	//histogram code
	if(cache_store->empty()){
		//logging code to alert client that
		//there's something wrong with the cache
	}
	else{
		for(auto it : *cache_store)
			it.hits = 0;
	}
	}
	//takes in a pointer to the id and output parameter for return found
	void Cache::hit(const uint64_t * obj, bool & found){
		
		std::vector<cache::Element>::iterator it = std::find_if(cache_store->begin(), cache_store->end(), Element(*obj)); 			    //edited the operator() to accept * to obj
		if(it != cache_store->end()){
		    it->hit();
		    found = true;
		}
		else{
		    found = false;     
		}
	}

	//finds the first instance of element, then pins it
	void Cache::pin_element(const uint64_t * obj){
			
		std::vector<cache::Element>::iterator it = std::find_if(cache_store->begin(), cache_store->end(), Element(*obj)); 			    //edited the operator() to accept * to obj
    	    if(it != cache_store->end())
		    it->pin_element();
	
	}

	//finds the first instance of element, then unpins it
	void Cache::unpin_element(const uint64_t * obj){
			
		std::vector<cache::Element>::iterator it = std::find_if(cache_store->begin(), cache_store->end(), Element(*obj)); 			    //edited the operator() to accept * to obj
    	    if(it != cache_store->end())
		    it->unpin_element();
	
	
	}
} //namespace cache
