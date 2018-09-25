// Include BeliefCache first to avoid boost conflicts
#define BELIEFCACHE_CEPH
#include "librbd/cache/ext/BeliefCache/src/VirtCache.h"

#include "include/assert.h"
#include "PredictionWorkQueue.h"
#include "PrefetchImageCache.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::PredictionWorkQueue: " << this << " " \
                           <<  __func__ << ": "
#define VIRTCACHE_SIZE 5
#define MAX_UNIQUE_ELEMENTS 20000

namespace librbd {

namespace cache {

PredictionWorkQueue::PredictionWorkQueue(std::string n, time_t ti,
    ThreadPool* p, std::function<void(uint64_t)> prefetch, CephContext* cct) :
        WorkQueueVal<uint64_t>(n, ti, 0, p), prefetch(prefetch), cct(cct),
        lock("PredictionWorkQueue::lock",
            true, false)
{
    ldout(cct, 20) << "Creating VirtCache" << dendl;
    //virt_cache = new beliefcache::VirtCache(std::numeric_limits<uint64_t>::max());
    virt_cache = new beliefcache::VirtCache(VIRTCACHE_SIZE, MAX_UNIQUE_ELEMENTS, cct);
}

uint64_t PredictionWorkQueue::encode_chunk(uint64_t chunk_id) {
    auto found = unique_chunk_map.find(chunk_id);

    if (found != unique_chunk_map.end()) {
	return found->second;
    }

    uint64_t id = unique_chunk_map.size();
    unique_chunk_map.insert_or_assign(chunk_id, id);
    return id;
}

uint64_t PredictionWorkQueue::decode_chunk(uint64_t chunk_id) {
    return unique_chunk_map[chunk_id];
}

void PredictionWorkQueue::_process(uint64_t id, ThreadPool::TPHandle &) {
    ldout(cct, 20) << "Adding element " << id << " to BeliefCache history" << dendl;

    lock.Lock();

    ldout(cct, 20) << "Updating virtual cache history" << dendl;
    virt_cache->updateHistory(encode_chunk(id));
    
    ldout(cct, 20) << "Virtual cache history updated" << dendl;
    auto& prefetch_list = virt_cache->getPrefetchList();

    ldout(cct, 20) << prefetch_list.size() << " elements in prefetch list" << dendl;

    for (auto i : prefetch_list) {
	uint64_t decoded_id = decode_chunk(i.id);
        ldout(cct, 20) << "chunk " << decoded_id << " appears in prefetch list" << dendl;
        prefetch(decoded_id);
    }

    prefetch_list.clear();

    ldout(cct, 20) << "Finished processing BeliefCache job" << dendl;
    
    lock.Unlock();
}

bool PredictionWorkQueue::_empty() {
    return jobs.empty();
}

void PredictionWorkQueue::_enqueue(uint64_t id) {
    jobs.push_back(id);
}

void PredictionWorkQueue::_enqueue_front(uint64_t id) {
    jobs.push_front(id);
}

uint64_t PredictionWorkQueue::_dequeue() {
    auto job = jobs.front();
    jobs.pop_front();
    return job;
}

    
}
}
