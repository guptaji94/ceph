#include "VirtCache.h"

// STL includes
#include <algorithm>
#include <iterator>

namespace predictcache{
	VirtCache::VirtCache() :
		cacheSize_(vcacheGlobals.vcache_size), stat_(), params_(vcacheGlobals)
	{
		elements_.reserve(cacheSize_);
	}

	VirtCache::VirtCache(CacheParameters params) :
		cacheSize_(params.vcache_size), stat_(), params_(params)
	{
		elements_.reserve(cacheSize_);
	}

	//copy constructor
	VirtCache::VirtCache(const VirtCache& temp)
	{
		cacheSize_ = temp.cacheSize_;
		unused_ = temp.unused_;
		insertions_ = temp.insertions_;
		elements_ = temp.elements_;
		usageHistogram_ = temp.usageHistogram_;

		stat_ = temp.stat_;
		params_ = temp.params_;
	}

	VirtCache& VirtCache::operator=(const VirtCache& other) {
		cacheSize_ = other.cacheSize_;
		unused_ = other.unused_;
		insertions_ = other.insertions_;
		elements_ = other.elements_;
		usageHistogram_ = other.usageHistogram_;

		stat_ = other.stat_;
		params_ = other.params_;
		stat_.setParameters(other.params_);

		return *this;
	}

	// Merge cache candidates and their updated values into the virtual cache
	// Generate prefetch list, eviction list, and compute statistics
    void VirtCache::merge(const std::vector<predictcache::Element>& cacheCandidates) {
		std::vector<Element> removedElements;

		std::vector<uint64_t> previousCacheElementIds, currentCacheElementIds, removedIds,
			prefetchIds;
		for (auto& i : elements_) {
			previousCacheElementIds.push_back(i.id);
		}

		{
			// Lock elements_ access
			std::lock_guard<std::mutex> lock(elementMutex_);

			for (auto& i : cacheCandidates) {
				// If already in elements, just update the belief value
				auto found = std::find_if(elements_.begin(), elements_.end(), [=](Element x){ return x.id == i.id; });
				if (found != elements_.end()) {
					found->belief = i.belief;
				} else {
					elements_.push_back(i);
				}
			}
			
			// Sort all cache elements by belief, highest to lowest
			std::sort(elements_.begin(), elements_.end(), [](Element a, Element b) { return a.belief > b.belief; });

			// Remove all but first cacheSize_ elements from the cache
			if (elements_.size() > cacheSize_) {
				std::copy(elements_.begin() + cacheSize_, elements_.end(), std::back_inserter(removedElements));
				elements_.erase(elements_.begin() + cacheSize_, elements_.end());
			}
		}

		for (auto& i : elements_) {
			currentCacheElementIds.push_back(i.id);
		}

		for (auto& i : removedElements) {
			removedIds.push_back(i.id);
		}

		std::copy_if(currentCacheElementIds.begin(), currentCacheElementIds.end(),
			std::back_inserter(prefetchIds),
			[=](uint64_t element){
				return std::find(previousCacheElementIds.begin(), previousCacheElementIds.end(),
					element) == previousCacheElementIds.end(); });

		std::copy_if(removedIds.begin(), removedIds.end(),
			std::back_inserter(evict_),
			[=](uint64_t element){
				return std::find(previousCacheElementIds.begin(), previousCacheElementIds.end(),
					element) != previousCacheElementIds.end(); });

		// Any removal candidates who end up in the evict ID list count for usage statistics
		for (auto& i : removedElements) {
			auto found = std::find(evict_.begin(), evict_.end(), i.id);

			if (found != evict_.end()) {
				if (i.hits == 0.0) {
					unused_++;
				}

				usageHistogram_[i.hits]++;
			}
		}

		/*
		// If any candidates have their IDs in the prefetch ID list, add them to the prefetch list
		for (auto& i : cacheCandidates) {
			auto found = std::find(prefetchIds.begin(), prefetchIds.end(), i.id);

			if (found != prefetchIds.end()) {
				std::lock_guard<std::mutex> lock(prefetch_.mutex);
				prefetch_.elements.push_back(i);
			}
		}*/

		// Update all elements
		{
			std::copy(elements_.begin(), elements_.end(),
				std::back_inserter(prefetch_));
		}

		insertions_ += prefetchIds.size();
	}

	bool VirtCache::hit(const uint64_t objId, const bool changeHitrate) {
		std::lock_guard<std::mutex> lock(elementMutex_);

		auto found = std::find_if(elements_.begin(), elements_.end(), [=](Element x){ return x.id == objId; });

		if (found == elements_.end()) {
			return false;
		} else {
			if (changeHitrate) {
				found->hits++;
			}
			return true;
		}
	}

	void VirtCache::finalizeHistogram() {
		for (auto i : elements_) {
			if (i.hits == 0.0) {
				unused_++;
			} else {
				usageHistogram_[i.hits]++;
			}
		}
	}
	double VirtCache::getBelief(uint64_t elementId) const {
		double belief = 0.0;
		auto found = std::find_if(elements_.begin(), elements_.end(),
			[=](Element a){ return a.id == elementId; });
		
		if(found != elements_.end())
			belief = found->belief;

		return belief;
	}

	uint64_t VirtCache::getNumUnusedElements() const { return unused_; }
	uint64_t VirtCache::getNumInsertions() const {return insertions_; }
	std::map<uint64_t, uint64_t>& VirtCache::getUsageHistogram() { return usageHistogram_; }

	CacheParameters VirtCache::getParameters() const {return params_;}

	void VirtCache::updateHistory(const uint64_t obj) {
		std::vector<Element> currentElements;
		currentElements.reserve(elements_.size());

		for (auto i : elements_) {
			currentElements.emplace_back(i.id, 0.0);
		}

		auto result = stat_.updateHistory(obj, currentElements);

		merge(result);
	}

	std::vector<uint64_t>& VirtCache::getEvictionList() {
		return evict_;
	}

	void VirtCache::updateParameters() {
		stat_.setParameters(params_);
	}

	
	
} //namespace cache	
