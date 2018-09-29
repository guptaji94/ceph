#ifndef CEPH_LIBRBD_CACHE_PREFETCH_REAL_CACHE
#define CEPH_LIBRBD_CACHE_PREFETCH_REAL_CACHE

#define CACHE_SIZE 5
#define CACHE_CHUNK_SIZE 1024

#include "common/dout.h"
#include "include/buffer.h"
#include <deque>
#include <unordered_map>

namespace librbd{
  namespace cache{
    class DetectionModule;

    using ElementID = uint64_t;
    using Extent = std::pair<uint64_t, uint64_t>;

    // map for cache entries
    using ImageCacheEntries = std::map<ElementID, bufferptr>;
 
    // list for handling LRU eviction 
    using LRUList = std::list<ElementID>;

    class RealCache{
      public:
        RealCache(DetectionModule* detection_wq, CephContext *m_cct);

        void insert(uint64_t image_extents_addr, bufferptr bl, bool copy_result);
  	bufferptr get(uint64_t image_extent_addr);

	static ElementID extent_to_unique_id(uint64_t extent_offset);
	static ElementID extent_to_unique_id(Extent extent);
	static Extent id_to_extent(ElementID id);

      private:
        DetectionModule* detection_wq;
        CephContext *m_cct;
        ImageCacheEntries cache_entries;
        LRUList lru_list;

        mutable Mutex lock;

        void updateLRUList(CephContext* m_cct, uint64_t cacheKey);
        void evictCache(CephContext* m_cct, uint64_t cacheKey);
    };
  } // namespace cache
} // namespace librbd
#endif
