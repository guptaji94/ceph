#ifndef CEPH_LIBRBD_CACHE_PREFETCH_REAL_CACHE
#define CEPH_LIBRBD_CACHE_PREFETCH_REAL_CACHE

#define CACHE_SIZE 1048576
#define CACHE_CHUNK_SIZE 1024

#include "common/dout.h"
#include "include/buffer.h"
#include <deque>
#include <unordered_map>

namespace librbd{
  namespace cache{
    class RealCache{
      public:
        static RealCache *Instance(CephContext *m_cct);
        void insert(CephContext* m_cct, uint64_t image_extents_addr, bufferptr bl);
  	bufferptr get(CephContext* m_cct, uint64_t image_extent_addr);

      private:
        RealCache(){};
        ~RealCache();
        CephContext *m_cct;
        typedef std::unordered_map<uint64_t, bufferptr> ImageCacheEntries;
        static RealCache *m_pInstance;
        ImageCacheEntries *cache_entries;
        typedef std::list<uint64_t> LRUList;
        LRUList *lru_list;
        void init(CephContext* m_cct);
        void updateLRUList(CephContext* m_cct, uint64_t cacheKey);
        void evictCache(CephContext* m_cct, uint64_t cacheKey);
    };
  } // namespace cache
} // namespace librbd
#endif
