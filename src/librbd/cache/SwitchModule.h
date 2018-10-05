#ifndef CEPH_LIBRBD_SWITCH_MODULE
#define CEPH_LIBRBD_SWITCH_MODULE

// STL includes
#include <deque>
#include <map>
#include <array>

// Ceph includes
#include "common/WorkQueue.h"

namespace librbd {
    namespace cache {
        /**
         * SwitchInput is used for any input that will be fed to the SwitchModule.
         * The enumerator Type is used to tell what kind of data is provided, and the Value
         * is a union which can either hold an unsigned integer, or a double, based on what
         * the type is. This allows for the input to remain small, as required for the
         * WorkQueueVal base class.
         */
        struct SwitchInput {
            struct CacheUpdate {
                uint64_t evicted;
                uint64_t inserted;
            };

            unsigned char cacheId;

            enum class Type {
                AccessStream,
                SwapElements,
                InsertElement,
                EvictElement
            };

            union Value {
                double d;
                uint64_t u;
                CacheUpdate update;
            };

            Type type;
            Value value;

            // Helper factory methods
            static SwitchInput AccessStream(uint64_t elementId);
            static SwitchInput SwapElements(unsigned char cacheId, uint64_t evictId,
                uint64_t insertId);
            static SwitchInput InsertElement(unsigned char cacheId,uint64_t insertId);
            static SwitchInput EvictElement(unsigned char cacheId,uint64_t evictId);
        };

        /**
         * SwitchModule is an object that takes certain inputs given by
         * a virtual cache, such as HitRate, UsageRation, etc. and determines
         * if the virtual cache is performing adequately. If it is not, the
         * Switch module will replace the active virtual cache with another,
         * and notify a retraining module.
         */
        class SwitchModule: public ThreadPool::WorkQueueVal<SwitchInput> {
            public:
                SwitchModule(std::string n, time_t ti, ThreadPool* p,
                    CephContext* cct);

            protected:
                void _process(SwitchInput input, ThreadPool::TPHandle &) override;

            private:
                std::deque<SwitchInput> jobs_;
                mutable Mutex lock;
                CephContext* cct_;

                // Number of times an element appeared in the access stream
                std::map<uint64_t, uint64_t> accessCounts_;

                // Last time an element appeared in the access stream, by access stream index
                std::map<uint64_t, uint64_t> lastAccess_;

                struct UsageStats_ {
                    // Elements currently in the virtual cache
                    std::vector<uint64_t> currentElements;

                    double frequency = 0.0;
                    double hitrate = 0.0;
                    uint64_t hits = 0;
                };

                // Updates the frequency of the current cache elements
                void updateFrequency(unsigned char cacheId);

                // Updates the recency using the most recent element
                void updateRecency(uint64_t recentElementId);

                // Stats
                uint64_t totalAccessCount_ = 0;
                double recency_ = 0.0;
                double recencyWeight_ = 0.5;
                std::array<UsageStats_, 2> usageStats_;

                // Functions for ceph::WorkQueueVal
                bool _empty() override;
                void _enqueue(SwitchInput val) override;
                void _enqueue_front(SwitchInput val) override;
                SwitchInput _dequeue() override;
        };
    }
}

#endif
