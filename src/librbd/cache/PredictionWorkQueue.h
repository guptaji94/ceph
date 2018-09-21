#ifndef CEPH_LIBRBD_PREDICTION_WORK_QUEUE
#define CEPH_LIBRBD_PREDICTION_WORK_QUEUE

#include "common/WorkQueue.h"

#include <vector>
#include <string>
#include <deque>

namespace librbd {

namespace cache {

    // A prediction work queue processes elements as jobs, and updates the
    // virtual cache asynchronously. When the BeliefCache calculations
    // are done, it notifies the real cache of advised prefetch elements
    // via the prefetch() callback function
class PredictionWorkQueue: public ThreadPool::WorkQueueVal<uint64_t> {
    public:
        PredictionWorkQueue(std::string n, time_t ti, ThreadPool* p,
        std::function<void(uint64_t)> prefetch, CephContext* cct);

    protected:
        void _process(uint64_t id, ThreadPool::TPHandle &) override;

    private:
        std::deque<uint64_t> jobs;

        CephContext* cct;
        std::function<void(uint64_t)> prefetch;

        bool _empty() override;
        void _enqueue(uint64_t val) override;
        void _enqueue_front(uint64_t val) override;
        uint64_t _dequeue() override;
};

}
}

#endif // CEPH_LIBRBD_PREDICTION_WORK_QUEUE
