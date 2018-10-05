#include "RealCache.h"

#include "common/dout.h"
#include "include/buffer.h"
#include "DetectionModule.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix                                                            \
  *_dout << "librbd::cache::RealCache: "                                       \
         << " " << __func__ << ": "
namespace librbd {
  namespace cache {
    RealCache::RealCache(DetectionModule* detection_wq, CephContext *m_cct) :
      detection_wq(detection_wq), m_cct(m_cct), lock("RealCache::lock", true, false)
    {
    }
 
    void RealCache::insert(uint64_t image_extents_addr, bufferptr bl, bool copy_result) {
  		ldout(m_cct, 20) << "inserting the image extent :: " << image_extents_addr << " bufferlist :: " << bl << " to cache " << dendl;

				if (copy_result) {
					ldout(m_cct, 20) << "copying " << bl.length() << " bytes of " << bl
													 << " to cache" << dendl;
					bufferptr b_copy(buffer::copy(bl.raw_c_str(), bl.length()));

					lock.Lock();
					updateLRUList(m_cct, image_extents_addr);
					cache_entries.insert_or_assign(image_extents_addr, b_copy);
					lock.Unlock();
				} else {
					ldout(m_cct, 20) << "storing " << bl.length() << " bytes of " << bl
													 << " in cache" << dendl;

					lock.Lock();
					updateLRUList(m_cct, image_extents_addr);
					cache_entries.insert_or_assign(image_extents_addr, bl);
					lock.Unlock();
				}
				
	
  		ldout(m_cct, 20) << "cache size after insert :: " << cache_entries.size() << dendl;
			ldout(m_cct, 20) << "Cache elements after insert :: " << dendl;

			for (auto i : cache_entries) {
				ldout(m_cct, 20) << i << dendl;
			}
    }

    bufferptr RealCache::get(uint64_t image_extent_addr){
  	ldout(m_cct, 20) << "reading from cache for the image extent :: " << image_extent_addr << dendl;

		total_requests ++;
		ldout(m_cct, 20) << "Total requests: " << total_requests << dendl;

    bufferptr bl;
  	auto cache_entry = cache_entries.find(image_extent_addr);
  	if(cache_entry == cache_entries.end()){
  		ldout(m_cct, 20) << "No match in cache for :: " << image_extent_addr << dendl;

			misses ++;
			hit_rate = static_cast<double>(hits) / static_cast<double>(misses);
			detection_wq->queue(DetectionInput::HitRate(hit_rate));

  		return bl;
  	} else {
  		bl = cache_entry->second;
  		bufferptr bl1 = bl;
  		ldout(m_cct, 20) << "Bufferlist from cache :: " << bl1 << dendl;
  		ldout(m_cct, 20) << "Image Extent :: " << cache_entry->first << " BufferList :: " << bl << dendl;
      updateLRUList(m_cct, cache_entry->first);

			hits ++;
			hit_rate = static_cast<double>(hits) / static_cast<double>(misses);
			detection_wq->queue(DetectionInput::HitRate(hit_rate));

  		return bl;
  	  }
    }


    ElementID RealCache::extent_to_unique_id(uint64_t extent_offset) {
			return (extent_offset / CACHE_CHUNK_SIZE) + 1;
    }

    ElementID RealCache::extent_to_unique_id(Extent extent) {
			return extent_to_unique_id(extent.first);
    }

    Extent RealCache::id_to_extent(ElementID id) {
			return std::make_pair((id + 1) * CACHE_CHUNK_SIZE, CACHE_CHUNK_SIZE);
    }


    void RealCache::updateLRUList(CephContext* m_cct, uint64_t cacheKey){
    	ldout(m_cct, 20) << "inside updateLRUList method" << dendl;

				auto result = std::find(lru_list.begin(), lru_list.end(), cacheKey);

				// If the LRU list is full and the element isn't already there...
      	if (result == lru_list.end() && lru_list.size() == CACHE_SIZE) {
        	uint64_t last = lru_list.back();

					// Remove last
        	lru_list.pop_back();
        	cache_entries.erase(last);

					// Insert the new element
					lru_list.push_front(cacheKey);

					// Notify the Detection module that we evicted last and added cacheKey to
					// the cache
					{
						DetectionInput input;
						input.type = DetectionInput::Type::SwapElements;
						input.value.update.evicted = last;
						input.value.update.inserted = cacheKey;

						detection_wq->queue(input);
					}
      	} else {
					
					// We're just moving it to the front if the element is already there
					if (result != lru_list.end()) {
        		lru_list.remove(cacheKey);
					} else {
						// Notify the detection module if the element is new to the LRU list/cache
						DetectionInput input;
						input.type = DetectionInput::Type::InsertElement;
						input.value.u = cacheKey;

						detection_wq->queue(input);
					}

        	lru_list.push_front(cacheKey);
      	}
      	ldout(m_cct, 20) << "new lru_list order :: " << lru_list << dendl;
    }

    void RealCache::evictCache(CephContext* m_cct, uint64_t cacheKey){
      	ldout(m_cct, 20) << "inside evictCache method" << dendl;
    }
  } // namespace cache
} // namespace librbd
