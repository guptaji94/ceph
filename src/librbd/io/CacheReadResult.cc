// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "librbd/io/CacheReadResult.h"
#include "librbd/io/AioCompletion.h"
#include "librbd/cache/PrefetchImageCache.h"
#include "librbd/ImageCtx.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::io::CacheReadResult: " << this \
                           << " " << __func__ << ": "

namespace librbd {
namespace io {

CacheReadResult::CacheReadResult(ceph::bufferlist *bl, ceph::bufferlist* src) :
    ReadResult(bl, src)
{}

CacheReadResult::CacheReadResult(CacheReadResult&& other) :
    ReadResult(std::move(other))
{}

CacheReadResult::C_ImageCacheReadRequest::C_ImageCacheReadRequest(
    AioCompletion *aio_completion, const Extents image_extents,
    std::vector<cache::ElementID> chunk_ids)

  : aio_completion(aio_completion), image_extents(image_extents), chunk_ids(chunk_ids) {

  CephContext *cct = aio_completion->ictx->cct;
  ldout(cct, 10) << "Entering CacheReadResult::C_ImageCacheReadRequest"
		  << dendl;

  aio_completion->add_request();

  ldout(cct, 10) << "Exiting CacheReadResult::C_ImageCacheReadRequest"
		 << dendl;

}

void CacheReadResult::C_ImageCacheReadRequest::finish(int r) {
  CephContext *cct = aio_completion->ictx->cct;
  ldout(cct, 20) << "C_ImageCacheReadRequest: r=" << r
                 << " from finish in CacheReadResult"
                 << " and completion=" << aio_completion <<dendl;

  if (r >= 0) {
    size_t length = 0;
    if (aio_completion->aio_type == io::AIO_TYPE_CACHE_READ) {
      ldout(cct, 20) << "cache read path triggered in read result" << dendl;
    }
    for (auto &image_extent : image_extents) {
      length += image_extent.second;
    }
    ldout(cct, 20) << "length=" << length << ", bl.length=" << bl.length()
                   << ", extents=" << image_extents
                   << ", bl=" << &bl << dendl;

    assert(length == bl.length());

    // If we're given chunk_ids, then we must write something to the cache
    if (chunk_ids.size() > 0) {
      ldout(cct, 20) << "updating real cache with chunk_ids=" << chunk_ids << dendl;
      auto imageCache = *(static_cast<librbd::cache::PrefetchImageCache<ImageCtx>*>((aio_completion->ictx->image_cache)));
      imageCache.aio_cache_returned_data(&bl, chunk_ids);
    } else {
      ldout(cct, 20) << "no chunk ids given - not updating real cache " << dendl;
    }

    ldout(cct, 20) << "bufferlist pointer before lock in CacheReadResult.cc:: " << bl << dendl;

    aio_completion->lock.Lock();
    aio_completion->read_result.m_destriper.add_partial_result(
                    cct, bl, image_extents);
    aio_completion->lock.Unlock();

    r = length;

    ldout(cct, 20) << "bufferlist pointer after lock in CacheReadResult.cc:: " << bl << dendl;

    ldout(cct, 20) << "r = " << r << " length = " << length << dendl;

  }

  aio_completion->complete_request(r);
  ldout(cct, 20) << "bufferlist length after complete_request(r) in CacheReadResult.cc :: " << bl.length() << dendl;
  ldout(cct, 20) << "bufferlist pointer after complete_request(r) in CacheReadResult.cc:: " << bl << dendl;
}

CacheReadResult::C_ObjectCacheReadRequest::C_ObjectCacheReadRequest(
    AioCompletion *aio_completion, uint64_t object_off, uint64_t object_len,
    Extents&& buffer_extents, std::vector<cache::ElementID> chunk_ids)
  : aio_completion(aio_completion), object_off(object_off),
    object_len(object_len), buffer_extents(std::move(buffer_extents)),
    chunk_ids(chunk_ids) {
  aio_completion->add_request();
}

void CacheReadResult::C_ObjectCacheReadRequest::finish(int r) {
  CephContext *cct = aio_completion->ictx->cct;
  ldout(cct, 10) << "C_ObjectCacheReadRequest: r=" << r
                 << dendl;
  if (aio_completion->aio_type == io::AIO_TYPE_CACHE_READ) {
      ldout(cct, 20) << "cache read path triggered in read result" << dendl;
  }

  if (r == -ENOENT) {
    r = 0;
  }
  if (r >= 0) {
    ldout(cct, 10) << " got " << extent_map
                   << " for " << buffer_extents
                   << " bl " << bl.length() << dendl;
    // handle the case where a sparse-read wasn't issued
    if (extent_map.empty()) {
      extent_map[object_off] = bl.length();
    }

    ldout(cct, 20) << "bl length before adding partial result=" << bl.length() << dendl;
    aio_completion->lock.Lock();
    aio_completion->read_result.m_destriper.add_partial_sparse_result(
      cct, bl, extent_map, object_off, buffer_extents);
    aio_completion->lock.Unlock();

    ldout(cct, 20) << "bl length after adding partial result=" << bl.length() << dendl;
    r = object_len;
  }

  auto imageCache =
    *(static_cast<librbd::cache::PrefetchImageCache<ImageCtx>*>((aio_completion->ictx->image_cache)));
    imageCache.aio_cache_returned_data(&bl, chunk_ids);

  aio_completion->complete_request(r);
}

} // namespace io
} // namespace librbd