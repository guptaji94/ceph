#include "DetectionModule.h"

// STL includes
#include <algorithm>

// Project includes
#include "Globals.h"

namespace predictcache {
    DetectionModule::DetectionModule(uint64_t numBuckets, std::function<void()> shiftCallback) :
        frequencyDetect_(numBuckets), recencyDetect_(numBuckets), hitrateDetect_(numBuckets),
        shiftCallback_(shiftCallback)
    {}

    void DetectionModule::updateHistory(uint64_t elementId, double hitrate) {
        // Thread safe
        std::lock_guard<std::mutex> lock(mutex_);

        // Check for existing elements, to ensure that every access count is initialized to 0
        auto found = accessCounts_.find(elementId);

        // Update access counts if necessary
        if (found == accessCounts_.end()) {
            accessCounts_[elementId] = 1;
        } else {
            found->second ++;
        }

        calculateFrequency(elementId);
        calculateRecency(elementId);

        hitrate_ = hitrate;

        // We must calculate recency before we update the last access
        lastAccess_[elementId] = totalAccesses_ ++;

        // If we've reached the end of a cycle, run phase detection
        if (++ticksSinceCycle_ >= beliefcache::adaptationGlobals.cycle_ticks) {
            processAccessStream();
            ticksSinceCycle_ = 0;
        }
    }

    double DetectionModule::getFrequency() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frequency_;
    }

    double DetectionModule::getRecency() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recency_;
    }

    void DetectionModule::calculateFrequency(uint64_t elementId) {
        uint64_t newCount = 0;

        // Check for existing elements, to prevent errors
        auto found = accessCounts_.find(elementId);

        // Use access count for this element
        if (found != accessCounts_.end()) {
            newCount = accessCounts_[elementId];
        }

        frequency_ = frequencyWeight * static_cast<double>(newCount) +
            (1.0 - frequencyWeight) * frequency_;
    }

    void DetectionModule::calculateRecency(uint64_t elementId) {
        // accessTime is maxAccessTime by default
        uint64_t accessTime = maxAccessTime;

        auto found = lastAccess_.find(elementId);

        if (found != lastAccess_.end()) {
            accessTime = totalAccesses_ - found->second;

            // accessTime is limited to maxAccessTime, even if found
            if (accessTime > maxAccessTime) {
                accessTime = maxAccessTime;
            }
        }

        recency_ = recencyWeight * static_cast<double>(accessTime) +
            (1.0 - recencyWeight) * recency_;
    }

    void DetectionModule::processAccessStream() {
        frequencyShift_ = frequencyDetect_.update(frequency_);
        recencyShift_ = frequencyDetect_.update(recency_);
        hitrateShift_ = frequencyDetect_.update(hitrate_);
        checkPhaseShift();
    }

    void DetectionModule::checkPhaseShift() {
        if (frequencyShift_ && recencyShift_ && hitrateShift_) {
            shiftCallback_();
        }
    }
}
