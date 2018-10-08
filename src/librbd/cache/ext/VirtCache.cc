#include "VirtCache.h"

// STL includes
#include <algorithm>
#include <iterator>

namespace predictcache{
	#ifndef PREDICTCACHE_CEPH
	VirtCache::VirtCache(const uint64_t cacheSize) :
		cacheSize_(cacheSize), stat_(), params_(vcacheGlobals)
	{
		elements_.reserve(cacheSize);
	}
	#else
	VirtCache::VirtCache(const uint64_t cacheSize, const CacheParameters params,
			CephContext* cct):
		cacheSize_(cacheSize), stat_(params, cct), params_(params),
		cct_(cct)
	{
		elements_.reserve(cacheSize);
	}
	#endif

	// Merge cache candidates and their updated values into the virtual cache
	// Generate prefetch list, eviction list, and compute statistics
    void VirtCache::merge(const std::vector<predictcache::Element>& cacheCandidates) {
		std::vector<Element> removedElements;

		std::vector<uint64_t> previousCacheElementIds, currentCacheElementIds, removedIds,
			prefetchIds;
		for (auto& i : elements_) {
			previousCacheElementIds.push_back(i.id);
		}

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

		// If any candidates have their IDs in the prefetch ID list, add them to the prefetch list
		for (auto& i : cacheCandidates) {
			auto found = std::find(prefetchIds.begin(), prefetchIds.end(), i.id);

			if (found != prefetchIds.end()) {
				prefetch_.push_back(i);
			}
		}

		insertions_ += prefetchIds.size();
	}

	bool VirtCache::hit(const uint64_t objId) {
		auto found = std::find_if(elements_.begin(), elements_.end(), [=](Element x){ return x.id == objId; });

		if (found == elements_.end()) {
			return false;
		} else {
			found->hits++;
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
	       
	uint64_t VirtCache::getNumUnusedElements() const { return unused_; }
	uint64_t VirtCache::getNumInsertions() const {return insertions_; }
	std::map<uint64_t, uint64_t>& VirtCache::getUsageHistogram() { return usageHistogram_; }

	void VirtCache::updateHistory(const uint64_t obj) {
		std::vector<Element> currentElements;
		currentElements.reserve(elements_.size());

		for (auto i : elements_) {
			currentElements.emplace_back(i.id, 0.0);
		}

		auto result = stat_.updateHistory(obj, currentElements);

		merge(result);
	}

	std::vector<Element>& VirtCache::getPrefetchList() {
		return prefetch_;
	}

	std::vector<uint64_t>& VirtCache::getEvictionList() {
		return evict_;
	}

	
	
} //namespace cache	
