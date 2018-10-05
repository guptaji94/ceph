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
        DetectionInput DetectionInput::HitRate(double hitrate) {
            DetectionInput input;
            input.type = DetectionInput::Type::HitRate;
            input.value.d = hitrate;

            return input;
        }

        DetectionInput DetectionInput::UsageRatio(double usageratio) {
            DetectionInput input;
            input.type = DetectionInput::Type::UsageRatio;
            input.value.d = usageratio;

            return input;
        }

        DetectionInput DetectionInput::AccessStream(uint64_t elementId) {
            DetectionInput input;
            input.type = DetectionInput::Type::AccessStream;
            input.value.u = elementId;

            return input;
        }

        DetectionInput DetectionInput::SwapElements(uint64_t evictId, uint64_t insertId) {
            DetectionInput input;
            input.type = DetectionInput::Type::SwapElements;
            input.value.update.evicted = evictId;
            input.value.update.inserted = insertId;

            return input;
        }

        DetectionInput DetectionInput::InsertElement(uint64_t insertId) {
            DetectionInput input;
            input.type = DetectionInput::Type::InsertElement;
            input.value.u = insertId;

            return input;
        }

        DetectionModule::DetectionModule(std::string n, time_t ti, ThreadPool* p, CephContext* cct) :
            WorkQueueVal<DetectionInput>(n, ti, 0, p),
            lock("DetectionModule::lock", true, false),
            cct_(cct)
        {
            ldout(cct_, 20) << "Creating detection module " << dendl;
        }

        void DetectionModule::_process(DetectionInput input, ThreadPool::TPHandle &) {
            switch (input.type) {
                case DetectionInput::Type::HitRate:
                {
                    hitrate_ = input.value.d;
                    updateHitrate();

                    break;
                }

                case DetectionInput::Type::AccessStream:
                {
                    uint64_t elementId = input.value.u;

                    lock.Lock();

                    auto result = accessCounts_.find(elementId);
                    if (result != accessCounts_.end()) {
                        accessCounts_[elementId] += 1;
                    } else {
                        accessCounts_[elementId] = 1;
                    }

                    lock.Unlock();

                    updateFrequency();
                    updateRecency(elementId);

                    lock.Lock();

                    lastAccess_[elementId] = totalAccessCount_;
                    totalAccessCount_ ++;

                    ldout(cct_, 20) << "Total requests: " << totalAccessCount_ << dendl;

                    lock.Unlock();

                    break;
                }

                case DetectionInput::Type::InsertElement:
                {
                    uint64_t insertId = input.value.u;

                    ldout(cct_, 20) << "Inserting " << insertId << " into cache elements" << dendl;

                    lock.Lock();
                    currentElements_.push_back(insertId);
                    lock.Unlock();
                    
                    updateFrequency();

                    break;
                }

                // Update local copy of cache elements
                case DetectionInput::Type::SwapElements:
                {
                    uint64_t evictId = input.value.update.evicted;
                    uint64_t insertId = input.value.update.inserted;

                    lock.Lock();

                    // Remove evicted ID
                    auto found = std::find(currentElements_.begin(), currentElements_.end(), evictId);
                    if (found != currentElements_.end()) {
                        currentElements_.erase(found);
                    }

                    // Insert inserted ID
                    currentElements_.push_back(insertId);

                    ldout(cct_, 20) << "Replacing cache element " << evictId << " with "
                                    << insertId << dendl;

                    lock.Unlock();

                    updateFrequency();

                    break;
                }

                default:
                    break;
            }

            // Determine if the cache is performing adequately here
        }

        void DetectionModule::updateHitrate() {
            ldout(cct_, 20) << "Hitrate: " << hitrate_ << dendl;
        }

        void DetectionModule::updateFrequency() {
            double totalAccesses = 0.0;

            lock.Lock();

            ldout(cct_, 20) << "Updating frequency: " << dendl;
            ldout(cct_, 20) << currentElements_.size() << " elements in the cache" << dendl;

            for (auto i : currentElements_) {
                auto found = accessCounts_.find(i);

                if (found != accessCounts_.end()) {
                    totalAccesses += static_cast<double>(found->second);
                    ldout(cct_, 20) << "Element " << i << " has an access count of "
                                    << found->second << dendl;
                } else {
                    ldout(cct_, 20) << "Element " << i << " has an access count of zero" << dendl;
                }
            }

            frequency_ = totalAccesses / static_cast<double>(currentElements_.size());

            lock.Unlock();

            ldout(cct_, 20) << "Frequency: " << frequency_ << dendl;
        }

        void DetectionModule::updateRecency(uint64_t recentElementId) {
            // If the element isn't in the access stream, it was mostly recently accessed at
            // MAX_RECENCY_TIME
            uint64_t accessTime = MAX_RECENCY_TIME;

            lock.Lock();

            auto result = lastAccess_.find(recentElementId);
            if (result != lastAccess_.end()) {
                accessTime = totalAccessCount_ - result->second;
            }

            lock.Unlock();

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
