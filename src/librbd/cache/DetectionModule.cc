#include "DetectionModule.h"

// STL includes
#include <algorithm>

// Enable Ceph debug output
#define dout_subsys ceph_subsys_rbd
#undef  dout_prefix
#define dout_prefix *_dout << "librbd::DetectionModule: " << this << " " \
                           <<  __func__ << ": "

#define MAX_RECENCY_TIME 50000

namespace librbd {
    namespace cache {
        DetectionModule::DetectionModule(std::string n, time_t ti, ThreadPool* p, CephContext* cct) :
            WorkQueueVal<DetectionInput>(n, ti, 0, p),
            lock("DetectionModule::lock", true, false),
            cct_(cct)
        {
            ldout(cct_, 20) << "Creating detection module " << dendl;
        }

        void DetectionModule::_process(DetectionInput input, ThreadPool::TPHandle &) {
            lock.Lock();

            updateRecency(input.elementId);
            updateFrequency(input.elementId);

            hitrate_ = input.hitrate;

            accessCounts_[input.elementId] ++;
            lastAccess_[input.elementId] = totalAccessCount_++;

            lock.Unlock();

            // Determine if the cache is performing adequately here
        }

        void DetectionModule::updateFrequency(uint64_t recentElementId) {
            double totalAccesses = 0.0;

            ldout(cct_, 20) << "Updating frequency: " << dendl;
            
            auto found = accessCounts_.find(recentElementId);

            if (found != accessCounts_.end()) {
                totalAccesses = static_cast<double>(found->second);
            }

            frequency_ = frequency_ * (1.0 - frequencyWeight_) + 
                totalAccesses * frequencyWeight_;

            ldout(cct_, 20) << "Frequency: " << frequency_ << dendl;
        }

        void DetectionModule::updateRecency(uint64_t recentElementId) {
            // If the element isn't in the access stream, it was mostly recently accessed at
            // MAX_RECENCY_TIME
            uint64_t accessTime = MAX_RECENCY_TIME;

            auto result = lastAccess_.find(recentElementId);
            if (result != lastAccess_.end()) {
                accessTime = totalAccessCount_ - result->second;
            }

            recency_ = recencyWeight_ * static_cast<double>(accessTime)
                + (1 - recencyWeight_) * recency_;

            ldout(cct_, 20) << "Recency: " << recency_ << dendl;
        }

        bool DetectionModule::_empty() {
            return jobs_.empty();
        }

        void DetectionModule::_enqueue(DetectionInput input) {
            jobs_.push_back(input);
        }

        void DetectionModule::_enqueue_front(DetectionInput input) {
            jobs_.push_front(input);
        }

        DetectionInput DetectionModule::_dequeue() {
            auto job = jobs_.front();
            jobs_.pop_front();
            return job;
        }
    }
}
