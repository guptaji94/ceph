#pragma once

// STL includes
#include <map>
#include <vector>
#include <mutex>
#include <functional>

// Library includes
#include "Adwin.h"

namespace predictcache {
    class DetectionModule {
        // The maximum value for the time between accesses, used in recency calculations.
        // If the element in question hasn't been accessed, or has been accessed more than
        // maxAccessTime ticks ago, then maxAccessTime is used instead.
        const uint64_t maxAccessTime = 20000;
        const double recencyWeight = 0.5;
        const double frequencyWeight = 0.5;

        public:
            DetectionModule(uint64_t numBuckets, std::function<void()> shiftCallback);

            // Update detection values based on the most recent element in the access stream
            // and the hitrate at the time
            void updateHistory(uint64_t elementId, double hitrate);

            double getFrequency() const;
            double getRecency() const;

        private:
            // Statistics
            double frequency_ = 0.0;
            double recency_ = 0.0;
            double hitrate_ = 0.0;
            uint64_t totalAccesses_ = 0;

            std::map<uint64_t, uint64_t> accessCounts_;
            std::map<uint64_t, uint64_t> lastAccess_;


            // Phase shift detection
            uint64_t ticksSinceCycle_ = 0;

            Adwin frequencyDetect_;
            Adwin recencyDetect_;
            Adwin hitrateDetect_;

            bool frequencyShift_ = false;
            bool recencyShift_ = false;
            bool hitrateShift_ = false;

            std::function<void()> shiftCallback_;

            // Mutex
            mutable std::mutex mutex_;



            void calculateFrequency(uint64_t elementId);
            void calculateRecency(uint64_t elementId);

            // Pushes frequency and recency to Adwin for phase shift detection
            void processAccessStream();

            // Pushes hitrate to Adwin for phase shift detection
            void processHitrateStream();

            // Notifies callback if phase for all three parameters has shifted
            void checkPhaseShift();
    };
}
