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
    RealCache *RealCache::m_pInstance = NULL;

    RealCache *RealCache::Instance(CephContext *m_cct) {
      ldout(m_cct, 20) << "get the real cache instance" << dendl;
      if (!m_pInstance) {
        ldout(m_cct, 20)
                        << "no active instances, creating a new real cache instance" << dendl;
        m_pInstance = new RealCache();
        m_pInstance->init(m_cct);
      }
      ldout(m_cct, 20) << "returning the real cache instance" << dendl;
      return m_pInstance;
    }

    RealCache::~RealCache(){
      if(!cache_entries){
        cache_entries->clear();
      }

      if(!lru_list){
        lru_list->clear();
      }
    }

    void RealCache::insert(CephContext* m_cct, uint64_t image_extents_addr, bufferptr bl) {
  	ldout(m_cct, 20) << "inserting the image extent :: " << image_extents_addr << " bufferlist :: " << bl << " to cache " << dendl;
  	if(!cache_entries) {
  		ldout(m_cct, 20) << "instantiating the cache_entries" << dendl;
  		cache_entries = new ImageCacheEntries();
  	}
	ldout(m_cct, 20) << "cache_entries is not null :: "<< cache_entries << dendl;
	if(cache_entries->size() == CACHE_SIZE) {
        	updateLRUList(m_cct, image_extents_addr);
        	cache_entries->insert_or_assign(image_extents_addr, bl);
      	} else {
        	cache_entries->insert_or_assign(image_extents_addr, bl);
       		updateLRUList(m_cct, image_extents_addr);
      	}
  	ldout(m_cct, 20) << "cache size after insert :: " << cache_entries->size() << dendl;
    }

    bufferptr RealCache::get(CephContext* m_cct, uint64_t image_extent_addr){
  	ldout(m_cct, 20) << "reading from cache for the image extent :: " << image_extent_addr << dendl;

      	bufferptr bl;
  	ImageCacheEntries::const_iterator cache_entry = cache_entries->find(image_extent_addr);
  	if(cache_entry == (cache_entries)->end()){
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

    void RealCache::init(CephContext* m_cct){
	ldout(m_cct, 20) << "instantiating the ImageCacheEntries" << dendl;
  	cache_entries = new ImageCacheEntries();
      	cache_entries->reserve(CACHE_SIZE);
      	lru_list = new LRUList();
    }

    void RealCache::updateLRUList(CephContext* m_cct, uint64_t cacheKey){
    	ldout(m_cct, 20) << "inside updateLRUList method" << dendl;
      	if (lru_list->size() == CACHE_SIZE) {
        	uint64_t last = lru_list->back();
        	lru_list->pop_back();
        	cache_entries->erase(last);
      	} else {
        	lru_list->remove(cacheKey);
        	lru_list->push_front(cacheKey);
      	}
      	ldout(m_cct, 20) << "new lru_list order :: " << *lru_list << dendl;
    }

    void RealCache::evictCache(CephContext* m_cct, uint64_t cacheKey){
      	ldout(m_cct, 20) << "inside evictCache method" << dendl;
    }
  } // namespace cache
} // namespace librbd
