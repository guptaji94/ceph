// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_IO_CACHE_READ_RESULT_H
#define CEPH_LIBRBD_IO_CACHE_READ_RESULT_H

#include "include/int_types.h"
#include "include/buffer_fwd.h"
#include "include/Context.h"
#include "librbd/io/Types.h"
#include "librbd/io/ReadResult.h"
#include "osdc/Striper.h"
#include "librbd/cache/RealCache.h"
#include <sys/uio.h>
#include <boost/variant/variant.hpp>

struct CephContext;

namespace librbd {

struct ImageCtx;

namespace io {

struct AioCompletion;
template <typename> struct ObjectReadRequest;

class CacheReadResult : public ReadResult {
public:
  CacheReadResult(ceph::bufferlist *bl, ceph::bufferlist* src = NULL);

  // Move constructor
  CacheReadResult(CacheReadResult&& other);

  struct C_ImageCacheReadRequest : public Context {
    AioCompletion *aio_completion;
    Extents image_extents;
    bufferlist bl;
    std::vector<cache::ElementID> chunk_ids;

    C_ImageCacheReadRequest(AioCompletion *aio_completion, const Extents image_extents,
      std::vector<cache::ElementID> chunk_ids);

    void finish(int r) override;
  };

  struct C_ObjectCacheReadRequest : public Context {
    AioCompletion *aio_completion;
    uint64_t object_off;
    uint64_t object_len;
    Extents buffer_extents;
    std::vector<uint64_t> chunk_ids;

    bufferlist bl;
    ExtentMap extent_map;

    C_ObjectCacheReadRequest(AioCompletion *aio_completion, uint64_t object_off,
                        uint64_t object_len, Extents&& buffer_extents,
                        std::vector<uint64_t> chunk_ids);

    void finish(int r) override;
  };
};

} // namespace io
} // namespace librbd

#endif // CEPH_LIBRBD_IO_CACHE_READ_RESULT_H