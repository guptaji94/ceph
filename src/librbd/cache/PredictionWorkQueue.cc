#include "librbd/cache/ext/BeliefCache/src/VirtCache.h"
#include "include/assert.h"

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
    ldout(cct, 20) << "Creating VirtCache" << dendl;
    //virt_cache = new beliefcache::VirtCache(std::numeric_limits<uint64_t>::max());
    virt_cache = new beliefcache::VirtCache(1048576);
}

void PredictionWorkQueue::_process(uint64_t id, ThreadPool::TPHandle &) {
    ldout(cct, 20) << "Adding element " << id << " to BeliefCache history" << dendl;

    lock.Lock();

    ldout(cct, 20) << "Updating virtual cache history" << dendl;
    virt_cache->updateHistory(id);
    
    ldout(cct, 20) << "Virtual cache history updated" << dendl;
    auto& prefetch_list = virt_cache->getPrefetchList();

    ldout(cct, 20) << prefetch_list.size() << " elements in prefetch list" << dendl;

    for (auto i : prefetch_list) {
        ldout(cct, 20) << "chunk " << i.id << " appears in prefetch list" << dendl;
        prefetch(i.id);
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
