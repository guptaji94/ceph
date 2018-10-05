#ifndef CEPH_LIBRBD_PREDICTION_WORK_QUEUE
#define CEPH_LIBRBD_PREDICTION_WORK_QUEUE

#include "common/WorkQueue.h"

#include <vector>
#include <string>
#include <deque>
#include <map>

namespace beliefcache {
    class VirtCache;
}

namespace librbd {

namespace cache {

class SwitchModule;

    // A prediction work queue processes elements as jobs, and updates the
    // virtual cache asynchronously. When the BeliefCache calculations
    // are done, it notifies the real cache of advised prefetch elements
    // via the prefetch() callback function
class PredictionWorkQueue: public ThreadPool::WorkQueueVal<uint64_t> {
    public:
        PredictionWorkQueue(std::string n, time_t ti, ThreadPool* p,
            std::function<void(uint64_t)> prefetch, bool advising,
            SwitchModule* switch_module, unsigned char cache_id,
            CephContext* cct);

    protected:
        void _process(uint64_t id, ThreadPool::TPHandle &) override;

    private:
        std::deque<uint64_t> jobs;

        std::function<void(uint64_t)> prefetch;
        bool advising;
        SwitchModule* switch_module;
        unsigned char cache_id;
        CephContext* cct;
        
        beliefcache::VirtCache* virt_cache;
        mutable Mutex lock;

        std::map<uint64_t, uint64_t> unique_chunk_map;
        uint64_t access_count = 0;

        uint64_t encode_chunk(uint64_t chunk_id);
        uint64_t decode_chunk(uint64_t chunk_id);

        bool _empty() override;
        void _enqueue(uint64_t val) override;
        void _enqueue_front(uint64_t val) override;
        uint64_t _dequeue() override;

        
};

}
}

#endif // CEPH_LIBRBD_PREDICTION_WORK_QUEUE
