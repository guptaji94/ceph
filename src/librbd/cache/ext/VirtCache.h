#ifndef VIRTUAL_CACHE_H
#define VIRTUAL_CACHE_H

// STL includes
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

// Project includes
#include "Element.h"
#include "VirtStat.h"
#include "Constants.h"
#include "Globals.h"

namespace predictcache
{
	class SwitchModule;

	// For now, VirtCache is standalone class, will later make base Cache class
	class VirtCache {
		// TODO: Fix to public access
		// Right now SwitchModule just changes parameters directly
		friend class SwitchModule;

	// With the change in interface, a lot of these functions could be made private
	public:
		VirtCache();
		VirtCache(CacheParameters parameters);
		VirtCache(const VirtCache &temp);
		void merge(const std::vector<Element>& cacheCandidates);

		VirtCache& operator=(const VirtCache& other);

		// Determine if the object is stored in the cache. If changeHitrate is set,
		// this will change the stored hitrate for overall runtime statistics. 
		bool hit(const uint64_t objId, bool changeHitrate = true);
		void finalizeHistogram();

		double getBelief(uint64_t elementId) const;
		uint64_t getNumUnusedElements() const;
		uint64_t getNumInsertions() const;
		std::map<uint64_t, uint64_t>& getUsageHistogram();

		void updateHistory(const uint64_t obj);
		std::vector<Element>& getPrefetchList();
		std::vector<uint64_t>& getEvictionList();
		CacheParameters getParameters() const;

	private:
		uint64_t cacheSize_;
		uint64_t unused_ = 0;
		uint64_t insertions_ = 0;
		std::vector<Element> elements_;
		std::vector<Element> prefetch_;
		std::vector<uint64_t> evict_;
		std::map<uint64_t, uint64_t> usageHistogram_;

		VirtStat stat_;
		CacheParameters params_;

		mutable std::mutex elementMutex_;

		// Notifies VirtStat of a parameter change
		void updateParameters();
	}; 
} //namespace cache

#endif //VIRTUAL_CACHE_H
