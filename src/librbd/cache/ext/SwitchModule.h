#pragma once

// STL includes
#include <map>
#include <vector>
#include <mutex>
#include <random>

// Project includes
#include "VirtCache.h"

namespace predictcache {
    class SwitchModule {
        public:
            SwitchModule(std::vector<VirtCache*> caches);

            void startEvaluation();
            void updateHistory(uint64_t objectId);
            bool isEvaluating() const {
                return evaluating_;
            }

        private:
            enum class Parameter_ {
                Threshold,
                WindowSize,
                CcSize,
                VotersSize,
                Invalid
            };

            static auto parameterToInt(Parameter_ x) {
                return static_cast<typename std::underlying_type<Parameter_>::type>(x);
            }

            uint64_t ticksSinceCycle_ = 0;
            uint64_t currentCycle_ = 0;
            Parameter_ cycleParameter_ = Parameter_::Threshold;
            bool evaluating_ = false;

            uint8_t numCaches_;
            std::vector<VirtCache*> caches_;
            std::vector<uint64_t> hits_;
            std::vector<double> hitrates_;

            mutable std::mutex mutex_;

            std::default_random_engine randEngine_;
            std::uniform_int_distribution<> randDist_;

            void setCacheParameters();
    };
}
