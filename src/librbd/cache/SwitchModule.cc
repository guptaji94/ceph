#include "SwitchModule.h"

// STL includes
#include <algorithm>

// Enable Ceph debug output
#define dout_subsys ceph_subsys_rbd
#undef  dout_prefix
#define dout_prefix *_dout << "librbd::SwitchModule: " << this << " " \
                           <<  __func__ << ": "

#define MAX_RECENCY_TIME 50000

namespace librbd {
    namespace cache {
        SwitchInput SwitchInput::AccessStream(uint64_t elementId) {
            SwitchInput input;
            input.type = SwitchInput::Type::AccessStream;
            input.value.u = elementId;

            return input;
        }

        SwitchInput SwitchInput::SwapElements(unsigned char cacheId, 
            uint64_t evictId, uint64_t insertId)
        {
            SwitchInput input;
            input.cacheId = cacheId;
            input.type = SwitchInput::Type::SwapElements;
            input.value.update.evicted = evictId;
            input.value.update.inserted = insertId;

            return input;
        }

        SwitchInput SwitchInput::InsertElement(unsigned char cacheId,
            uint64_t insertId)
        {
            SwitchInput input;
            input.cacheId = cacheId;
            input.type = SwitchInput::Type::InsertElement;
            input.value.u = insertId;

            return input;
        }

        SwitchInput SwitchInput::EvictElement(unsigned char cacheId,
            uint64_t evictId)
        {
            SwitchInput input;
            input.cacheId = cacheId;
            input.type = SwitchInput::Type::EvictElement;
            input.value.u = evictId;

            return input;
        }

        SwitchModule::SwitchModule(std::string n, time_t ti, ThreadPool* p, CephContext* cct) :
            WorkQueueVal<SwitchInput>(n, ti, 0, p),
            lock("SwitchModule::lock", true, false),
            cct_(cct)
        {
            ldout(cct_, 20) << "Creating Switch module " << dendl;
        }

        void SwitchModule::_process(SwitchInput input, ThreadPool::TPHandle &) {
            switch (input.type) {
                case SwitchInput::Type::AccessStream:
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

                    for (unsigned char i = 0; i < 2; i++) {
                        updateFrequency(i);

                        lock.Lock();
                        auto found = std::find(usageStats_[i].currentElements.begin(),
                            usageStats_[i].currentElements.end(), elementId);

                        if (found != usageStats_[i].currentElements.end()) {
                            usageStats_[i].hits ++;
                        }

                        usageStats_[i].hitrate = static_cast<double>(usageStats_[i].hits) /
                            static_cast<double>(totalAccessCount_);

                        ldout(cct_, 20) << "Hitrate for virtual cache " << static_cast<double>(i)
                                        << ": " << usageStats_[i].hitrate << dendl;
                        lock.Unlock();
                    }

                    updateRecency(elementId);

                    lock.Lock();

                    lastAccess_[elementId] = totalAccessCount_;
                    totalAccessCount_ ++;

                    ldout(cct_, 20) << "Total requests: " << totalAccessCount_ << dendl;

                    lock.Unlock();

                    // We could compare the hitrates of the two virtual caches here
                    // and then switch which one is active

                    break;
                }

                case SwitchInput::Type::InsertElement:
                {
                    uint64_t insertId = input.value.u;

                    ldout(cct_, 20) << "Inserting " << insertId << " into cache "
                                    << input.cacheId << " elements" << dendl;

                    lock.Lock();
                    usageStats_[input.cacheId].currentElements.push_back(insertId);
                    lock.Unlock();
                    
                    updateFrequency(input.cacheId);

                    break;
                }

                // Update local copy of cache elements
                case SwitchInput::Type::SwapElements:
                {
                    uint64_t evictId = input.value.update.evicted;
                    uint64_t insertId = input.value.update.inserted;

                    lock.Lock();

                    auto& elements = usageStats_[input.cacheId].currentElements;

                    // Remove evicted ID
                    auto found = std::find(elements.begin(), elements.end(), evictId);
                    if (found != elements.end()) {
                        elements.erase(found);
                    }

                    // Insert inserted ID
                    elements.push_back(insertId);

                    ldout(cct_, 20) << "Replacing cache " << input.cacheId << " element "
                                    << evictId << " with " << insertId << dendl;

                    lock.Unlock();

                    updateFrequency(input.cacheId);

                    break;
                }

                default:
                    break;
            }

            // Determine if the cache is performing adequately here
        }

        void SwitchModule::updateFrequency(unsigned char cacheId) {
            double totalAccesses = 0.0;

            lock.Lock();

            auto& elements = usageStats_[cacheId].currentElements;

            ldout(cct_, 20) << "Updating frequency: " << dendl;
            ldout(cct_, 20) << elements.size() << " elements in the cache" << dendl;

            for (auto i : elements) {
                auto found = accessCounts_.find(i);

                if (found != accessCounts_.end()) {
                    totalAccesses += static_cast<double>(found->second);
                    ldout(cct_, 20) << "Element " << i << " has an access count of "
                                    << found->second << dendl;
                } else {
                    ldout(cct_, 20) << "Element " << i << " has an access count of zero" << dendl;
                }
            }

            usageStats_[cacheId].frequency = totalAccesses / static_cast<double>(elements.size());

            lock.Unlock();

            ldout(cct_, 20) << "Frequency " << cacheId << ": "
                            << usageStats_[cacheId].frequency << dendl;
        }

        void SwitchModule::updateRecency(uint64_t recentElementId) {
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

        bool SwitchModule::_empty() {
            return jobs_.empty();
        }

        void SwitchModule::_enqueue(SwitchInput input) {
            jobs_.push_back(input);
        }

        void SwitchModule::_enqueue_front(SwitchInput input) {
            jobs_.push_front(input);
        }

        SwitchInput SwitchModule::_dequeue() {
            auto job = jobs_.front();
            jobs_.pop_front();
            return job;
        }
    }
}
