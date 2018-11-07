#include "RealCache.h"

// STL includes
#include <algorithm>

// Project includes
#include "Globals.h"

namespace predictcache {
	RealCache::RealCache(uint64_t size) :
		size_(size)
	{}

	bool RealCache::hit(uint64_t objectId) {
		auto found = std::find_if(cache_.begin(), cache_.end(),
			[=](Element& e){return e.id == objectId;});
		
		if (found != cache_.end()) {
			found->hits++;
			return true;
		} else {
			return false;
		}
	}

	void RealCache::insertElementLRU(uint64_t objectId) {
		uint64_t hits = 0;

		auto found = std::find_if(cache_.begin(), cache_.end(),
			[=](Element& e){return e.id == objectId;});

		// If element is already there, remove it
		if (found != cache_.end()) {
			hits = found->hits;
			cache_.erase(found);
		} else {
		// Otherwise we have an insertion
			numInsertions_++;
		}

		// Add element to the front
		Element inserted(objectId);
		inserted.hits = hits;
		cache_.push_front(inserted);

		// Remove the last element if we've overexpanded
		if (cache_.size() > size_) {
			auto last = cache_.back();
			usageHistogram_[last.hits]++;

			cache_.pop_back();
		}
	}

	void RealCache::insertElementBelief(Element object) {
		auto found = std::find_if(cache_.begin(), cache_.end(),
			[=](Element& e){return e.id == object.id;});

		// If the element already exists, update belief
		if (found != cache_.end()) {
			found->belief = object.belief;
		} else {
			cache_.push_front(object);
		}

		std::sort(cache_.begin(), cache_.end(), [](Element a, Element b){return a.belief > b.belief;});

		if (cache_.size() > size_) {
			cache_.pop_back();
		}
	}

	void RealCache::finalizeHistogram() {
		for (auto& i : cache_) {
			usageHistogram_[i.hits]++;
		}
	}
}