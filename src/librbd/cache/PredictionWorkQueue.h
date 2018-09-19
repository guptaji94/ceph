#ifndef CEPH_LIBRBD_PREDICTION_WORK_QUEUE
#define CEPH_LIBRBD_PREDICTION_WORK_QUEUE

#include "common/WorkQueue.h"

#include <vector>
#include <string>
#include <deque>

namespace librbd {

namespace cache {

class PredictionWorkQueue: public ThreadPool::WorkQueueVal<uint64_t> {
    public:
        PredictionWorkQueue(std::string n, time_t ti, ThreadPool* p, CephContext* cct);

    protected:
        void _process(uint64_t id, ThreadPool::TPHandle &) override;

    private:
        std::deque<uint64_t> jobs;

        CephContext* cct;
        //VirtualCache* cache;

        bool _empty() override;
        void _enqueue(uint64_t val) override;
        void _enqueue_front(uint64_t val) override;
        uint64_t _dequeue() override;
};

}
}

#endif // CEPH_LIBRBD_PREDICTION_WORK_QUEUE
