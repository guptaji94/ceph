#ifndef CEPH_LIBRBD_PREDICTION_WORK_QUEUE
#define CEPH_LIBRBD_PREDICTION_WORK_QUEUE

#include "common/WorkQueue.h"

#include <vector>
#include <string>
#include <deque>
#include <map>

namespace predictcache {
    class VirtCache;
    class SwitchModule;
}

namespace librbd {

namespace cache {

struct PredictionInput {
    uint64_t elementId;
    bool phaseChangeDetected;

    PredictionInput(uint64_t elementId) :
        elementId(elementId), phaseChangeDetected(false)
    {}

    static PredictionInput PhaseChange() {
        PredictionInput input(0);
        input.phaseChangeDetected = true;

        return input;
    }
};

    // A prediction work queue processes elements as jobs, and updates the
    // virtual cache asynchronously. When the BeliefCache calculations
    // are done, it notifies the real cache of advised prefetch elements
    // via the prefetch() callback function
class PredictionWorkQueue: public ThreadPool::WorkQueueVal<PredictionInput> {
    public:
        PredictionWorkQueue(std::string n, time_t ti, ThreadPool* p,
            std::function<void(uint64_t)> prefetch,
            CephContext* cct);

    protected:
        void _process(PredictionInput job, ThreadPool::TPHandle &) override;

    private:
        std::deque<PredictionInput> jobs;

        std::function<void(uint64_t)> prefetch;
        CephContext* cct;
        
        predictcache::VirtCache* virt_cache;
        predictcache::VirtCache* virt_cache_secondary;
        predictcache::SwitchModule* switch_module;

        mutable Mutex lock;

        std::map<uint64_t, uint64_t> unique_chunk_map;
        uint64_t access_count = 0;

        uint64_t encode_chunk(uint64_t chunk_id);
        uint64_t decode_chunk(uint64_t chunk_id);

        bool _empty() override;
        void _enqueue(PredictionInput val) override;
        void _enqueue_front(PredictionInput val) override;
        PredictionInput _dequeue() override;

        
};

}
}

#endif // CEPH_LIBRBD_PREDICTION_WORK_QUEUE
