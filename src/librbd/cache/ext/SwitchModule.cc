#include "SwitchModule.h"

// STL includes
#include <iostream>

namespace predictcache {
    SwitchModule::SwitchModule(std::vector<VirtCache*> caches) :
        numCaches_(caches.size()), caches_(caches), hits_(numCaches_, 0),
        hitrates_(numCaches_, 0.0), randDist_(0, 1)
    {
    }

    void SwitchModule::startEvaluation() {
        ticksSinceCycle_ = 0;
        currentCycle_ = 0;
        cycleParameter_ = static_cast<Parameter_>(0);
        evaluating_ = true;
    }

    void SwitchModule::updateHistory(uint64_t objectId) {
        // Skip if we're not currently evaluating
        if (!evaluating_) {
            return;
        }

        for (uint8_t i = 0; i < numCaches_; i++) {
            if (caches_[i]->hit(objectId, false)) {
                hits_[i]++;
            }
        }

        // At the end of a cycle, do performance checks
        if (++ticksSinceCycle_ >= adaptationGlobals.cycle_ticks) {
            ticksSinceCycle_ = 0;

            // Calculate hitrates and clear hit counter
            for (uint8_t i = 0; i < numCaches_; i++) {
                hitrates_[i] = static_cast<double>(hits_[i]) /
                    static_cast<double>(adaptationGlobals.cycle_ticks);

                hits_[i] = 0;
            }

            // Find the best performing cache
            uint8_t bestCacheIndex = static_cast<uint8_t>(std::distance(hitrates_.begin(),
                std::max_element(hitrates_.begin(), hitrates_.end())));


            for (uint8_t i = 0; i < numCaches_; i++) {
                if (i != bestCacheIndex) {
                    // Replace other caches with best performing cache
                    *(caches_[i]) = *(caches_[bestCacheIndex]);
                }
            }

            // Go to next cache parameter
            cycleParameter_ = static_cast<Parameter_>(parameterToInt(cycleParameter_) + 1);

            // If we reached the last parameter, reset the parameter
            if (cycleParameter_ == Parameter_::Invalid) {
                cycleParameter_ = static_cast<Parameter_>(0);

                // Increment cycle
                currentCycle_++;

                // Check if cycle max has been reached
                if (currentCycle_ >= adaptationGlobals.cycles_per_test) {
                    evaluating_ = false;
                }
            }

            // Update parameters for the testing cache
            setCacheParameters();
        }
    }

    void SwitchModule::setCacheParameters() {
        bool increment = static_cast<bool>(randDist_(randEngine_));

        switch(cycleParameter_) {
            case Parameter_::Threshold:
                // Randomly increment or decrement
                caches_[1]->params_.thresh += (increment ? 0.01 : -0.01);
                
                // Wrap threshold between 0.06 and 0.17
                if (caches_[1]->params_.thresh < 0.06) {
                    caches_[1]->params_.thresh = 0.17;
                } else if (caches_[1]->params_.thresh > 0.17) {
                    caches_[1]->params_.thresh = 0.06;
                }
                break;

            case Parameter_::WindowSize:
                // Randomly increment or decrement
                caches_[1]->params_.window_size += (increment ? 1 : -1);

                // Wrap window size between 1 and 30
                if (caches_[1]->params_.window_size < 1) {
                    caches_[1]->params_.window_size = 30;
                } else if (caches_[1]->params_.window_size > 30) {
                    caches_[1]->params_.window_size = 1;
                }
                break;

            case Parameter_::CcSize:
                // Randomly increment or decrement
                caches_[1]->params_.cc_size += (increment ? 1 : -1);

                // Wrap cache candidates size between 1 and cache_size
                if (caches_[1]->params_.cc_size < 1) {
                    caches_[1]->params_.cc_size = caches_[1]->params_.vcache_size;
                } else if (caches_[1]->params_.cc_size > caches_[1]->params_.vcache_size) {
                    caches_[1]->params_.cc_size = 1;
                }
                break;

            case Parameter_::VotersSize:
                // Randomly increment or decrement
                caches_[1]->params_.voter_size += (increment ? 1 : -1);

                // Wrap voters size between 1 and 30
                if (caches_[1]->params_.voter_size < 1) {
                    caches_[1]->params_.voter_size = 30;
                } else if (caches_[1]->params_.voter_size > 30) {
                    caches_[1]->params_.voter_size = 1;
                }
                break;

            default:
                // Should be unreachable
                break;
        }

        caches_[1]->updateParameters();
    }
}
