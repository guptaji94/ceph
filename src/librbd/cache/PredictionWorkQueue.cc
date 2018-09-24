#include "librbd/cache/ext/BeliefCache/src/VirtCache.h"

#include "PredictionWorkQueue.h"

#include <limits>

#include "PrefetchImageCache.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::PredictionWorkQueue: " << this << " " \
                           <<  __func__ << ": "

namespace librbd {

namespace cache {

PredictionWorkQueue::PredictionWorkQueue(std::string n, time_t ti,
    ThreadPool* p, std::function<void(uint64_t)> prefetch, CephContext* cct) :
        WorkQueueVal<uint64_t>(n, ti, 0, p), prefetch(prefetch), cct(cct),
        lock("PredictionWorkQueue::lock",
            true, false)
{
    virt_cache = new beliefcache::VirtCache(std::numeric_limits<uint64_t>::max());
}

void PredictionWorkQueue::_process(uint64_t id, ThreadPool::TPHandle &) {
    ldout(cct, 20) << "Adding element " << id << " to BeliefCache history" << dendl;

    lock.Lock();

    virt_cache->updateHistory(id);
    auto& prefetch_list = virt_cache->getPrefetchList();

    for (auto i : prefetch_list) {
        prefetch(i.id);
    }

    prefetch_list.clear();
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
