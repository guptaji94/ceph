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
            WorkQueueVal<DetectionInput>(n, ti, 0, p), cct_(cct)
        {
            ldout(cct_, 20) << "Creating detection module " << dendl;
        }

        void DetectionModule::_process(DetectionInput input, ThreadPool::TPHandle &) {
            switch (input.type) {
                case DetectionInput::Type::HitRate:
                {
                    double hit_rate = input.value.d;
                    // Do something

                    break;
                }

                case DetectionInput::Type::AccessStream:
                {
                    uint64_t elementId = input.value.u;

                    auto result = accessCounts_.find(elementId);
                    if (result != accessCounts_.end()) {
                        accessCounts_[elementId] += 1;
                    } else {
                        accessCounts_[elementId] = 1;
                    }

                    updateFrequency();
                    updateRecency(elementId);

                    lastAccess_[elementId] = totalAccessCount_;
                    totalAccessCount_ ++;

                    break;
                }

                case DetectionInput::Type::InsertElement:
                {
                    uint64_t insertId = input.value.u;

                    currentElements_.push_back(insertId);
                    updateFrequency();
                }

                // Update local copy of cache elements
                case DetectionInput::Type::SwapElements:
                {
                    uint64_t evictId = input.value.update.evicted;
                    uint64_t insertId = input.value.update.inserted;

                    // Remove evicted ID
                    std::remove_if(currentElements_.begin(), currentElements_.end(),
                        [=](uint64_t e){ return e == evictId; });

                    // Insert inserted ID
                    currentElements_.push_back(insertId);
                    updateFrequency();

                    break;
                }

                default:
                    break;
            }
        }

        void DetectionModule::updateFrequency() {
            double totalAccesses = 0.0;

            for (auto i : currentElements_) {
                auto found = accessCounts_.find(i);

                if (found != accessCounts_.end()) {
                    totalAccesses += static_cast<double>(found->second);
                }
            }

            frequency_ = totalAccesses / static_cast<double>(currentElements_.size());

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
