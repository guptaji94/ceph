#pragma once

// STL includes
#include <vector>
#include <deque>
#include <map>

// Project includes
#include "Element.h"

namespace predictcache {
	class RealCache {
		public:
			RealCache(uint64_t size);

			void insertElementLRU(uint64_t objectId);
			void insertElementBelief(Element object);
			bool hit(uint64_t objectId);
			void finalizeHistogram();

			uint64_t getNumInsertions() const {
				return numInsertions_;
			}
			
			std::map<uint64_t, uint64_t>& getUsageHistogram() {
				return usageHistogram_;
			}

		private:
			uint64_t size_;
			uint64_t numInsertions_ = 0;
			std::deque<Element> cache_;
			std::map<uint64_t, uint64_t> usageHistogram_;

	};
}
