// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "PrefetchImageCache.h"
#include "include/buffer.h"
#include "common/dout.h"
#include "librbd/ImageCtx.h"
#include "librbd/io/CacheReadResult.h"

#include "PredictionWorkQueue.h"
#include "DetectionModule.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::PrefetchImageCache: " << this << " " \
                           <<  __func__ << ": "

#define writes_until_cache 389
#define do_prefetching true

namespace librbd {
namespace cache {

template <typename I>
PrefetchImageCache<I>::PrefetchImageCache(ImageCtx &image_ctx)
  : m_image_ctx(image_ctx), m_image_writeback(image_ctx) 
   {
  CephContext *cct = m_image_ctx.cct;

  // Get the thread pool for this context
  ContextWQ* op_work_queue;
  ThreadPool* thread_pool;
  m_image_ctx.get_thread_pool_instance(cct, &thread_pool, &op_work_queue);

  detection_wq = new DetectionModule("librbd::detection_module",
    cct->_conf->get_val<int64_t>("rbd_op_thread_timeout"),
    thread_pool, cct);
  
  real_cache = new RealCache(detection_wq, cct);

  ldout(m_image_ctx.cct, 20) << "Creating PredictionWorkQueue" << dendl;

  prediction_wq1 = new PredictionWorkQueue("librdb::prediction_work_queue",
    cct->_conf->get_val<int64_t>("rbd_op_thread_timeout"),
    thread_pool, std::bind(&PrefetchImageCache::prefetch_chunk, this, std::placeholders::_1), true,
    0, cct);

  prediction_wq2 = new PredictionWorkQueue("librdb::prediction_work_queue",
    cct->_conf->get_val<int64_t>("rbd_op_thread_timeout"),
    thread_pool, std::bind(&PrefetchImageCache::prefetch_chunk, this, std::placeholders::_1), false,
    1, cct);
}

template <typename I>
PrefetchImageCache<I>::~PrefetchImageCache() {
  delete prediction_wq2;
  delete prediction_wq1;
  delete real_cache;
  delete detection_wq;
}


/*  what PrefetchImageCache<I>::aio_read should do!! :
 *  get called (by ImageReadRequest::send_image_cache_request())
 *  get the image exents, and loop through, for each extent: split?? into 
 *    either each extent that comes in is split into its own sub list of offsets of CACHE_CHUNK_SIZE chunks, then these are all checked from the cache, or
 *    maybe, the whole thing gets split into a new image_extents that's just all the incoming extents split into chunks in one big list, then that gets checked?
 *    kinda doesn't matter
 *  then we dispatch the async eviction list updater
 *  but in the end, we have a lot of chunks, and for each one we check the cache - first lock it, then
 *  we use the count(key) method and see if that offset is in the cache, 
 *    if so we get that value which is a bufferlist*
 *    then we copy that back into the read bufferlist
 *  if it's not in the cache, 
 *    do that read from m_image_writeback, that should put it into the read bufferlist (???) 
 *    then we also put it in the cache - somehow this needs to happen
*/  


template <typename I>
void PrefetchImageCache<I>::aio_read(Extents &&image_extents, bufferlist *bl,
                                        int fadvise_flags, Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "image_extents=" << image_extents << ", "
                 << "on_finish=" << on_finish << dendl;

  read_count++;

  // Wait until caching flag is set, so that we can write test data without
  // interrupting process by caching
  if (caching) {
    std::vector<Extents> unique_list_of_extents;
    std::set<uint64_t> set_tracker;

    //get the extents, then call the chunking function
    std::vector<Extents> temp;
    for(auto &it : image_extents) {
      temp.push_back(extent_to_chunks(it));
    }

    ldout(cct, 25) << "\"temp\" after all extents chunked: "
      << temp << dendl;

    //loops through the row
    for (const auto &row : temp) {

      //temp list of extent
      Extents fogRow;

      //loops through the column
      for (const auto &s : row) {
        //inserts into a set and checks to see if the element is already inserted
        auto ret = set_tracker.insert(s.first);
        //if inserted, insert into the vector of list
        if (ret.second==true) {
          fogRow.push_back(s);
        }
      }
      unique_list_of_extents.push_back(fogRow);
    }

    if(unique_list_of_extents[0].size() > 1) {
      unique_list_of_extents[0].pop_back();
    }
    Extents correct_image_extents = unique_list_of_extents[0];
    ldout(cct, 20) << "fixed extent list: " << correct_image_extents << dendl;

    // This bufferlist should hold the data that the user requested which was already
    // in the cache
    ceph::bufferlist* cached_data = new ceph::bufferlist();

    // Figure out what isn't in the cache!
    std::vector<ElementID> uncached_chunk_ids;
    std::vector<Extent> uncached_chunk_extents;

    uint64_t num_cached_chunks = 0;

    for (auto i : correct_image_extents) {
      ElementID chunk_id = RealCache::extent_to_unique_id(i.first);

      if (do_prefetching) {
        // Add each chunk ID to the prediction and detection work queues
        prediction_wq1->queue(chunk_id);
        prediction_wq2->queue(chunk_id);
      }

      // And try to get it from the cache
      bufferptr cache_chunk_buffer = real_cache->get(chunk_id);

      if (cache_chunk_buffer.length() > 0) {
        // Cached chunks will get copied into the result buffer later
        cached_data->push_back(cache_chunk_buffer);
        num_cached_chunks ++;
      } else {
        // Uncached chunks will have to be requested from the cluster
        uncached_chunk_ids.push_back(chunk_id);
        uncached_chunk_extents.push_back(i);
      }
    }


    ldout(cct, 20) << "Number of requested chunks to be fetched= "
      << uncached_chunk_ids.size()
      << "Number of requested chunks in the cache= "
      << num_cached_chunks << dendl;

    // If any of the chunks aren't in the cache, we need to make an asynchronous request
    // from the cluster
    if (!uncached_chunk_ids.empty()) {
      // Create a callback to notify the LRU list/cache the IDs of the chunks we added
      Context* combined_on_finish = new CacheUpdate<I>(this, uncached_chunk_ids, cct, bl,
        true, on_finish);

      // If we have cached data that we need to copy into the result buffer, make sure
      // it's added to the callback
      ceph::bufferlist* copy_src = nullptr;
      if (num_cached_chunks > 0) {
        copy_src = cached_data;
      }

      auto aio_comp = io::AioCompletion::create_and_start(combined_on_finish, &m_image_ctx,
                                                            io::AIO_TYPE_READ);
      io::ImageReadRequest<I> req(m_image_ctx, aio_comp, std::move(uncached_chunk_extents),
                                  io::ReadResult{bl, copy_src}, fadvise_flags, {});
                                  
      req.set_bypass_image_cache();
      req.send();
    } else {
      ldout(cct, 20) << "All requested chunks are in cache "
                    << "Performing synchronous copy "
                    << "Requested extents=" << image_extents << " "
                    << "cached_data.length=" << cached_data->length() << " "
                    << "bl.length=" << bl->length() << dendl;
                

      // Otherwise synchronously copy from our cache chunks to the result buffer
      uint64_t total_request_size = cached_data->length();
      
      // ---NOTE: This appender isn't actually copying the data - fix it!
      // (Forgot to remove this comment?)
      {
        auto appender = bl->get_contiguous_appender(total_request_size, true);
        appender.append(*cached_data);
      }

      ldout(cct, 20) << "bl.length after copying from cache " << bl->length() << dendl;


      // NOTE: not sure what parameter this should take
      // -1 will cause ReadResult to not check the size of the buffer
      // but also not to invoke destriper - (is this bad?)
      on_finish->complete(bl->length());
    }
  } else {
    Extents extents_copy = image_extents;

    ldout(cct, 20) << "uncached request " << ", "
                   << "extents=" << image_extents << dendl;

    auto aio_comp = io::AioCompletion::create_and_start(on_finish, &m_image_ctx,
                                                            io::AIO_TYPE_READ);
    io::ImageReadRequest<I> req(m_image_ctx, aio_comp, std::move(extents_copy),
                                  io::ReadResult{bl}, fadvise_flags, {});  

    req.set_bypass_image_cache();
    req.send();
  }

  ldout(cct, 20) << "read_count=" << read_count << dendl;
}

template <typename I>
ImageCache::Extents PrefetchImageCache<I>::extent_to_chunks(std::pair<uint64_t, uint64_t> one_extent) {

  Extents chunkedExtent;
  uint64_t changedOffset;
  uint64_t changedLength;
  uint64_t offset = one_extent.first;
  uint64_t length = one_extent.second;
 
  if (offset%CACHE_CHUNK_SIZE != 0) {
    changedOffset = offset-offset%CACHE_CHUNK_SIZE;
    chunkedExtent.push_back(std::make_pair(changedOffset,CACHE_CHUNK_SIZE));
  } else {
    changedOffset = offset;
    chunkedExtent.push_back(std::make_pair(changedOffset,CACHE_CHUNK_SIZE));
  }         
  if ((length%CACHE_CHUNK_SIZE + offset) < CACHE_CHUNK_SIZE) {
    uint64_t remains2 = CACHE_CHUNK_SIZE - length%CACHE_CHUNK_SIZE;
    changedLength = length + remains2;
  } else if((offset%CACHE_CHUNK_SIZE + length) > CACHE_CHUNK_SIZE){
    uint64_t remains = CACHE_CHUNK_SIZE - length%CACHE_CHUNK_SIZE;
    changedLength = length+remains;
  } else { 
    changedOffset = offset;
    changedLength = length;
  }
  if (changedLength > CACHE_CHUNK_SIZE) {
    while (changedLength > CACHE_CHUNK_SIZE) {
      changedOffset += CACHE_CHUNK_SIZE;
      changedLength -= CACHE_CHUNK_SIZE;
      chunkedExtent.push_back(std::make_pair(changedOffset,CACHE_CHUNK_SIZE));
    }
  } else {
    chunkedExtent.push_back(std::make_pair(changedOffset,CACHE_CHUNK_SIZE));
  }
  return chunkedExtent;
                      
}

  
template <typename I>
void PrefetchImageCache<I>::aio_write(Extents &&image_extents,
                                         bufferlist&& bl,
                                         int fadvise_flags,
                                         Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;

  // debugging stats
  write_count++;

  ldout(cct, 20) << "image_extents=" << image_extents << ", "
                 << "on_finish=" << on_finish << ", "
                 << "write_count=" << write_count << dendl;

  if (write_count == writes_until_cache) {
    ldout(cct, 5) << "Starting caching after " << writes_until_cache
                  << " writes" << dendl;
    
    caching = true;
  }

  m_image_writeback.aio_write(std::move(image_extents), std::move(bl),
                              fadvise_flags, on_finish);
}

template <typename I>
void PrefetchImageCache<I>::aio_discard(uint64_t offset, uint64_t length,
                                           bool skip_partial_discard, Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "offset=" << offset << ", "
                 << "length=" << length << ", "
                 << "on_finish=" << on_finish << dendl;

  m_image_writeback.aio_discard(offset, length, skip_partial_discard, on_finish);
}

template <typename I>
void PrefetchImageCache<I>::aio_flush(Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "on_finish=" << on_finish << dendl;

  m_image_writeback.aio_flush(on_finish);
}

template <typename I>
void PrefetchImageCache<I>::aio_writesame(uint64_t offset, uint64_t length,
                                             bufferlist&& bl, int fadvise_flags,
                                             Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "offset=" << offset << ", "
                 << "length=" << length << ", "
                 << "data_len=" << bl.length() << ", "
                 << "on_finish=" << on_finish << dendl;

  m_image_writeback.aio_writesame(offset, length, std::move(bl), fadvise_flags,
                                  on_finish);
}

template <typename I>
void PrefetchImageCache<I>::aio_compare_and_write(Extents &&image_extents,
                                                     bufferlist&& cmp_bl,
                                                     bufferlist&& bl,
                                                     uint64_t *mismatch_offset,
                                                     int fadvise_flags,
                                                     Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "image_extents=" << image_extents << ", "
                 << "on_finish=" << on_finish << dendl;

  m_image_writeback.aio_compare_and_write(
    std::move(image_extents), std::move(cmp_bl), std::move(bl), mismatch_offset,
    fadvise_flags, on_finish);
}

template <typename I>
void PrefetchImageCache<I>::init(Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;    //for logging purposes
  ldout(cct, 20) << dendl;


  on_finish->complete(0);

  // init() called where? which context to use for on_finish?
  // in other implementations, init() is called in OpenRequest, totally unrelated
  // to constructor, which makes sense - image context is 'owned' by program
  // using librbd, so lifecycle is not quite the same as the actual open image connection

  // see librbd/Utils.h line 132 for create_context_callback which seems to be the kind of context that's needed for on_finish
}

template <typename I>
void PrefetchImageCache<I>::init_blocking() {
  CephContext *cct = m_image_ctx.cct;    //for logging purposes
  ldout(cct, 20) << "BLOCKING init without callback, being called from image cache constructor" << dendl;
}
  
  
template <typename I>
void PrefetchImageCache<I>::shut_down(Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << dendl;
}
  
  
template <typename I>
void PrefetchImageCache<I>::invalidate(Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << dendl;

  // dump cache contents (don't have anything)
  on_finish->complete(0);
}

template <typename I>
void PrefetchImageCache<I>::flush(Context *on_finish) {
  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << dendl;

  // internal flush -- nothing to writeback but make sure
  // in-flight IO is flushed
  aio_flush(on_finish);
}

/**
 * This method wil accept:
 * 1. Data returned from a request to the cluster made by aio_read
 * 2. Medata
 *
 * Add the data to cache with appropriate state.
 */
template <typename I>
void PrefetchImageCache<I>::aio_cache_returned_data( // const Extents& image_extents,
  ceph::bufferlist *bl, std::vector<cache::ElementID> chunk_ids, bool copy_result) {

  CephContext *cct = m_image_ctx.cct;
  ldout(cct, 20) << "caching the returned data "
                  // << " image_extents :: " << image_extents
                  << " bufferlist :: " << bl
                  << dendl;

	ceph::bufferlist bl1 = *bl;

 // ldout(cct, 20) << "buffers :: " << bl1.buffers() << dendl;

  ldout(cct, 20) << "length=" << chunk_ids.size() << ", bl.length=" << bl1.length()
                 << ", buffers=" << bl1.get_num_buffers() << dendl;

  if (chunk_ids.size() == bl1.get_num_buffers()) {
    uint64_t i = 0;
    for (auto& buffer : bl1.buffers()) {
      // if (in_prefetch_list(chunk_ids[i])) {
        real_cache->insert(chunk_ids[i], buffer, copy_result);
      // }
      i ++;
    }
  } else {
    ldout(cct, 20) << "Image extents and buffer list size is not same :: "
                          //  << "image_extents.size() :: " << image_extents.size()
                           << "bufferlist length :: " << bl1.length()
                           << dendl;
  }
}

template <typename I>
void PrefetchImageCache<I>::update_cache(std::vector<uint64_t> elements) {
  // Actually update the LRU/cache list here
  CephContext *cct = m_image_ctx.cct;
  for (auto i : elements) {
    ldout(cct, 20) << "Element " << i << " added to cache list" << dendl;
  }
}

template <typename I>
void PrefetchImageCache<I>::prefetch_chunk(uint64_t id) {
  ldout(m_image_ctx.cct, 20) << "Prefetching chunk " << id << dendl;

  bufferlist* cache_storage = new bufferlist();

  Extents extents {std::make_pair((id - 1) * CACHE_CHUNK_SIZE, CACHE_CHUNK_SIZE)};
  Context* on_completion = new CacheUpdate<I>(this, std::vector<uint64_t>{id}, m_image_ctx.cct,
                                              cache_storage, false);
  auto aio_comp = io::AioCompletion::create_and_start(on_completion, &m_image_ctx,
                                                        io::AIO_TYPE_READ);
  io::ImageReadRequest<I> req(m_image_ctx, aio_comp, std::move(extents),
                              io::ReadResult{cache_storage}, 0, {});
                              
  req.set_bypass_image_cache();
  req.send();
}


template <typename T>
CacheUpdate<T>::CacheUpdate(PrefetchImageCache<T>* cache, const std::vector<uint64_t>& elements,
    CephContext* cct, ceph::bufferlist* bl, bool copy_result, Context* to_run) :
  cache(cache), elements(elements), cct(cct), bl(bl), copy_result(copy_result), to_run(to_run)
{}


template <typename T>
void CacheUpdate<T>::finish(int r) {
  cache->aio_cache_returned_data(bl, elements, copy_result);

  if (to_run) {
    to_run->complete(r);
  }
}

} // namespace cache
} // namespace librbd

template class librbd::cache::PrefetchImageCache<librbd::ImageCtx>;
