#include "RealCache.h"
#include "common/dout.h"
#include "include/buffer.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix                                                            \
  *_dout << "librbd::cache::RealCache: "                                       \
         << " " << __func__ << ": "
namespace librbd {
  namespace cache {
    RealCache::RealCache(CephContext *m_cct) :
      m_cct(m_cct)
    {
    }
 
    void RealCache::insert(uint64_t image_extents_addr, bufferptr bl) {
  	ldout(m_cct, 20) << "inserting the image extent :: " << image_extents_addr << " bufferlist :: " << bl << " to cache " << dendl;

				bufferptr b_copy(buffer::copy(bl.raw_c_str(), bl.length()));

        updateLRUList(m_cct, image_extents_addr);
        cache_entries.insert_or_assign(image_extents_addr, b_copy);
	
  	ldout(m_cct, 20) << "cache size after insert :: " << cache_entries.size() << dendl;
    }

    bufferptr RealCache::get(uint64_t image_extent_addr){
  	ldout(m_cct, 20) << "reading from cache for the image extent :: " << image_extent_addr << dendl;

      	bufferptr bl;
  	auto cache_entry = cache_entries.find(image_extent_addr);
  	if(cache_entry == cache_entries.end()){
  		ldout(m_cct, 20) << "No match in cache for :: " << image_extent_addr << dendl;
  		return bl;
  	} else {
  		bl = cache_entry->second;
  		bufferptr bl1 = bl;
  		ldout(m_cct, 20) << "Bufferlist from cache :: " << bl1 << dendl;
  		ldout(m_cct, 20) << "Image Extent :: " << cache_entry->first << " BufferList :: " << bl << dendl;
        	updateLRUList(m_cct, cache_entry->first);
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
      	if (lru_list.size() == CACHE_SIZE) {
        	uint64_t last = lru_list.back();
        	lru_list.pop_back();
        	cache_entries.erase(last);
      	} else {
        	lru_list.remove(cacheKey);
        	lru_list.push_front(cacheKey);
      	}
      	ldout(m_cct, 20) << "new lru_list order :: " << lru_list << dendl;
    }

    void RealCache::evictCache(CephContext* m_cct, uint64_t cacheKey){
      	ldout(m_cct, 20) << "inside evictCache method" << dendl;
    }
  } // namespace cache
} // namespace librbd
