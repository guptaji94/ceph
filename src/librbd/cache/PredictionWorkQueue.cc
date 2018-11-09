// Include PredictCache first to avoid boost conflicts
#define PREDICTCACHE_CEPH
#include "librbd/cache/ext/VirtCache.h"
#include "librbd/cache/ext/SwitchModule.h"

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
    ThreadPool* p, std::function<void(uint64_t)> prefetch,
    CephContext* cct) :
        WorkQueueVal<PredictionInput>(n, ti, 0, p), prefetch(prefetch),
        cct(cct),
        lock("PredictionWorkQueue::lock",
            true, false)
{
    ldout(cct, 20) << "Creating VirtCache" << dendl;
    //virt_cache = new predictcache::VirtCache(std::numeric_limits<uint64_t>::max());
    predictcache::CacheParameters params;
    params.vcache_size = 5;
    params.max_matrix_size = MAX_UNIQUE_ELEMENTS;

    virt_cache = new predictcache::VirtCache(params);
    virt_cache_secondary = new predictcache::VirtCache(params);

    std::vector<predictcache::VirtCache*> caches = {virt_cache, virt_cache_secondary};
    switch_module = new predictcache::SwitchModule(caches);
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
    auto found = std::find_if(unique_chunk_map.begin(), unique_chunk_map.end(),
	[=](auto x){ return x.second == chunk_id; });

    if (found != unique_chunk_map.end()) {
	return found->second;
    }

    ldout(cct, 20) << "Error - no chunk mapped to unique ID "
		   << chunk_id << dendl;

    return 0;
}

void PredictionWorkQueue::_process(PredictionInput job, ThreadPool::TPHandle &) {
    if (job.phaseChangeDetected) {
        // Start retraining!
        switch_module->startEvaluation();
        return;
    }

    ldout(cct, 20) << "Adding element " << job.elementId << " to VirtCache history" << dendl;

    lock.Lock();

    ldout(cct, 20) << "Updating virtual cache history" << dendl;
    uint64_t encoded_id = encode_chunk(job.elementId);

    virt_cache->updateHistory(encoded_id);

    if (switch_module->isEvaluating()) {
        ldout(cct, 20) << "Updating secondary virtual cache history "
                       << "and retraining module" << dendl;

        virt_cache_secondary->updateHistory(encoded_id);
        switch_module->updateHistory(encoded_id);
    }
    
    ldout(cct, 20) << "Virtual cache history updated" << dendl;
    auto& prefetch_list = virt_cache->getPrefetchList();

    ldout(cct, 20) << prefetch_list.size() << " elements in prefetch list" << dendl;

    for (auto i : prefetch_list) {
	    uint64_t decoded_id = decode_chunk(i.id);
        ldout(cct, 20) << "chunk " << decoded_id << " appears in prefetch list" << dendl;

        ldout(cct, 20) << "Advising real cache to prefetch " << decoded_id << dendl;
        prefetch(decoded_id);
    }

    prefetch_list.clear();

    auto& evict_list = virt_cache->getEvictionList();
    ldout(cct, 20) << evict_list.size() << " elements in eviction list" << dendl;

    evict_list.clear();

    ldout(cct, 20) << "Finished processing VirtCache job" << dendl;

    access_count++;
    ldout(cct, 20) << "Total requests: " << access_count << dendl;
    
    lock.Unlock();
}

bool PredictionWorkQueue::_empty() {
    return jobs.empty();
}

void PredictionWorkQueue::_enqueue(PredictionInput job) {
    jobs.push_back(job);
}

void PredictionWorkQueue::_enqueue_front(PredictionInput job) {
    jobs.push_front(job);
}

PredictionInput PredictionWorkQueue::_dequeue() {
    auto job = jobs.front();
    jobs.pop_front();
    return job;
}

    
}
}
