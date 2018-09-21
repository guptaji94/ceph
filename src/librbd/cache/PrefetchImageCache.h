// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_CACHE_PREFETCH_IMAGE_CACHE
#define CEPH_LIBRBD_CACHE_PREFETCH_IMAGE_CACHE
#define HASH_SIZE 1048576
#define CACHE_CHUNK_SIZE 1024

#include "ImageCache.h"
#include "ImageWriteback.h"
#include "librbd/io/AioCompletion.h"
#include "librbd/io/ImageRequest.h"

#include <deque>
#include <unordered_map>
#include <vector>

namespace librbd {

struct ImageCtx;

namespace cache {

class PredictionWorkQueue;

/**
 * Example passthrough client-side, image extent cache
 * ---- modified, now using for prefetch cache
 */
template <typename ImageCtxT = librbd::ImageCtx>
class PrefetchImageCache : public ImageCache {
public:
  explicit PrefetchImageCache(ImageCtx &image_ctx);
  ~PrefetchImageCache();

  /// client AIO methods
  void aio_read(Extents&& image_extents, ceph::bufferlist *bl,
                int fadvise_flags, Context *on_finish) override;
  void aio_write(Extents&& image_extents, ceph::bufferlist&& bl,
                 int fadvise_flags, Context *on_finish) override;
  void aio_discard(uint64_t offset, uint64_t length,
                   bool skip_partial_discard, Context *on_finish) override;
  void aio_flush(Context *on_finish) override;
  void aio_writesame(uint64_t offset, uint64_t length,
                     ceph::bufferlist&& bl,
                     int fadvise_flags, Context *on_finish) override;
  void aio_compare_and_write(Extents&& image_extents,
                             ceph::bufferlist&& cmp_bl, ceph::bufferlist&& bl,
                             uint64_t *mismatch_offset,int fadvise_flags,
                             Context *on_finish) override;

  void update_cache(std::vector<uint64_t> ids);
  void prefetch_chunk(uint64_t id);

  /// internal state methods
  void init(Context *on_finish) override;
  void init_blocking() override;
  void shut_down(Context *on_finish) override;

  void invalidate(Context *on_finish) override;
  void flush(Context *on_finish) override;
  
  

private:
  ImageCtxT &m_image_ctx;
  ImageWriteback<ImageCtxT> m_image_writeback;

  // map for cache entries
  typedef std::unordered_map<uint64_t, ceph::bufferlist *> ImageCacheEntries;
  ImageCacheEntries *cache_entries;

  // queue for handling LRU eviction 
  typedef std::deque<uint64_t> LRUQueue;
  LRUQueue *lru_queue;

  Extents extent_to_chunks(std::pair<uint64_t, uint64_t> image_extents); 

  // Work queue to handle Belief value calculations
  PredictionWorkQueue* prediction_wq;

};

// CacheUpdate is a callback function (otherwise known as a Context in Ceph)
// that tells the image cache to update its cache/LRU list, as well as run
// the additional callback specified in its construction
template <typename T>
class CacheUpdate: public Context {
  public:
    CacheUpdate(PrefetchImageCache<T>* cache, const std::vector<uint64_t>& elements,
      CephContext* cct, Context* to_run = nullptr);
    void finish(int r) override;

  private:
    PrefetchImageCache<T>* cache;
    std::vector<uint64_t> elements;
    CephContext* cct;
    Context* to_run;
};
	

} // namespace cache
} // namespace librbd

extern template class librbd::cache::PrefetchImageCache<librbd::ImageCtx>;

#endif // CEPH_LIBRBD_CACHE_PREFETCH_IMAGE_CACHE
