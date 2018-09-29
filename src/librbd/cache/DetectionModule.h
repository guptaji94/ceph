#ifndef CEPH_LIBRBD_DETECTION_MODULE
#define CEPH_LIBRBD_DETECTION_MODULE

// STL includes
#include <deque>
#include <map>

// Ceph includes
#include "common/WorkQueue.h"

namespace librbd {
    namespace cache {
        /**
         * DetectionInput is used for any input that will be fed to the DetectionModule.
         * The enumerator Type is used to tell what kind of data is provided, and the Value
         * is a union which can either hold an unsigned integer, or a double, based on what
         * the type is. This allows for the input to remain small, as required for the
         * WorkQueueVal base class.
         */
        struct DetectionInput {
            struct CacheUpdate {
                uint64_t evicted;
                uint64_t inserted;
            };

            enum class Type {
                HitRate,
                UsageRatio,
                AccessStream,
                SwapElements,
                InsertElement
            };

            union Value {
                double d;
                uint64_t u;
                CacheUpdate update;
            };

            Type type;
            Value value;
        };

        /**
         * DetectionModule is an object that takes certain inputs given by
         * a virtual cache, such as HitRate, UsageRation, etc. and determines
         * if the virtual cache is performing adequately. If it is not, the
         * detection module will replace the active virtual cache with another,
         * and notify a retraining module.
         */
        class DetectionModule: public ThreadPool::WorkQueueVal<DetectionInput> {
            public:
                DetectionModule(std::string n, time_t ti, ThreadPool* p,
                    CephContext* cct);

            protected:
                void _process(DetectionInput input, ThreadPool::TPHandle &) override;

            private:
                std::deque<DetectionInput> jobs_;
                CephContext* cct_;

                // Number of times an element appeared in the access stream
                std::map<uint64_t, uint64_t> accessCounts_;

                // Last time an element appeared in the access stream, by access stream index
                std::map<uint64_t, uint64_t> lastAccess_;

                // Elements currently in the real cache
                std::vector<uint64_t> currentElements_;

                // Stats
                uint64_t totalAccessCount_ = 0;
                double frequency_ = 0.0;
                double recency_ = 0.0;
                double recencyWeight_ = 0.5;

                // Updates the frequency of the current cache elements
                void updateFrequency();

                // Updates the recency using the most recent element
                void updateRecency(uint64_t recentElementId);

                // Functions for ceph::WorkQueueVal
                bool _empty() override;
                void _enqueue(DetectionInput val) override;
                void _enqueue_front(DetectionInput val) override;
                DetectionInput _dequeue() override;
        };
    }
}

#endif
