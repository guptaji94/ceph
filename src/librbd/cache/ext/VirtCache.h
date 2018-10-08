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

#ifndef PREDICTCACHE_SIM
#define PREDICTCACHE_CEPH
#endif

#ifdef PREDICTCACHE_CEPH
#include "librbd/ImageCtx.h"
#endif

namespace predictcache
{
	// For now, VirtCache is standalone class, will later make base Cache class
	class VirtCache {

	// With the change in interface, a lot of these functions could be made private
	public:
		#ifndef PREDICTCACHE_CEPH
		VirtCache(const uint64_t cacheSize);
		#else
		VirtCache(const uint64_t cacheSize, const CacheParameters params,
			CephContext* cct);
		#endif
		void merge(const std::vector<Element>& cacheCandidates);	 
		uint64_t getBelief(const uint64_t objId);
		bool hit(const uint64_t objId);
		void finalizeHistogram();

		uint64_t getNumUnusedElements() const;
		uint64_t getNumInsertions() const;
		std::map<uint64_t, uint64_t>& getUsageHistogram();

		void updateHistory(const uint64_t obj);
		std::vector<Element>& getPrefetchList();
		std::vector<uint64_t>& getEvictionList();

	private:
		uint64_t cacheSize_;
		uint64_t unused_ = 0;
		uint64_t insertions_ = 0;

		std::vector<Element> elements_, prefetch_;
		std::vector<uint64_t> evict_;
		std::map<uint64_t, uint64_t> usageHistogram_;

		VirtStat stat_;

		CacheParameters params_;

		#ifdef PREDICTCACHE_CEPH
		CephContext* cct_;
		#endif
	}; 
} //namespace cache

#endif //VIRTUAL_CACHE_H
