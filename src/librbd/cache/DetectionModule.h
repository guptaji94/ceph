#ifndef CEPH_LIBRBD_DETECTION_MODULE
#define CEPH_LIBRBD_DETECTION_MODULE

// STL includes
#include <deque>
#include <map>

// Ceph includes
#include "common/WorkQueue.h"

// ext includes
#include "librbd/cache/ext/adwin/C++/Adwin.h"

namespace librbd {
    namespace cache {
        /**
         * DetectionInput is used for any input that will be fed to the DetectionModule.
         * It holds both the hitrate at the time the element appeared in the stream, and the
         * ID of the element in the access stream.
         */
        struct DetectionInput {
            double hitrate;
            uint64_t elementId;

            DetectionInput(double hitrate, uint64_t elementId) :
                hitrate(hitrate), elementId(elementId)
            {};
        };

        /**
         * DetectionModule is an object that takes the access stream and the hitrate at the correct
         * tick, and determines if there has been a phase shift in the workload. It then notifies
         * the SwitchModule to begin retraining. 
         */
        class DetectionModule: public ThreadPool::WorkQueueVal<DetectionInput> {
            public:
                DetectionModule(std::string n, time_t ti, ThreadPool* p,
                    std::function<void()> detectionCallback, CephContext* cct,
                    uint64_t ticksPerCycle = 100, uint64_t detectionBuckets = 50);

            protected:
                void _process(DetectionInput input, ThreadPool::TPHandle &) override;

            private:
                std::deque<DetectionInput> jobs_;
                std::function<void()> detectionCallback_;

                mutable Mutex lock;
                CephContext* cct_;

                // Number of times an element appeared in the access stream
                std::map<uint64_t, uint64_t> accessCounts_;

                // Last time an element appeared in the access stream, by access stream index
                std::map<uint64_t, uint64_t> lastAccess_;


                // Stats
                uint64_t totalAccessCount_ = 0;
                double frequency_ = 0.0;
                double recency_ = 0.0;
                double hitrate_ = 0.0;
                double frequencyWeight_ = 0.5;
                double recencyWeight_ = 0.5;


                // Adwin
                uint64_t ticksPerCycle_;
                Adwin frequencyDetect_;
                Adwin recencyDetect_;
                Adwin hitrateDetect_;

                // Updates the frequency using the most recent element
                void updateFrequency(uint64_t recentElementId);

                // Updates the recency using the most recent element
                void updateRecency(uint64_t recentElementId);

                // Checks if a phase shift has occurred using Adwin
                void checkPhase();

                // Functions for ceph::WorkQueueVal
                bool _empty() override;
                void _enqueue(DetectionInput val) override;
                void _enqueue_front(DetectionInput val) override;
                DetectionInput _dequeue() override;
        };
    }
}

#endif
