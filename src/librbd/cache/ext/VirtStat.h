#ifndef VIRT_STAT_H
#define VIRT_STAT_H

// STL includes
#include <string>
#include <cstdint>
#include <vector>

// Boost includes
#include <boost/numeric/ublas/matrix_sparse.hpp>

// Project includes

#include "Element.h"
#include "Globals.h"

namespace predictcache {

	using ObjectID = uint64_t;

	template <typename T>
	using SparseMatrix = boost::numeric::ublas::mapped_matrix<T>;

	class VirtStat{
		public:
			VirtStat();

			// Calling update history will add obj to VirtStat's stored history
			// and perform all the belief calculations as needed
			// it will then return a list of Elements with the appropriate
			// belief values ready to be dispatched to the prefetch list
			// including most recent maximal belief for cacheElements
			// by VirtCache
			std::vector<Element> updateHistory(ObjectID obj, std::vector<Element>& cacheElements);

			void updateCounts(ObjectID obj, ObjectID next);
			void calculateSignificance(ObjectID first);

			// Calculates significance, but only for P[voter,candidate]
			void calculateSignificance(std::vector<ObjectID>::iterator votersStart,
				std::vector<ObjectID>::iterator votersEnd,
				std::vector<Element>::iterator candidatesStart,
				std::vector<Element>::iterator candidatesEnd);

			void findMaxBelief(std::vector<Element>& candidates, std::vector<ObjectID>::iterator votersStart,
				std::vector<ObjectID>::iterator votersEnd, double threshold);
			std::vector<Element> findCacheCandidates(const ObjectID obj, uint64_t numCandidates);

			void setParameters(CacheParameters params);
			
			const SparseMatrix<uint64_t>& getAccessStats() const;
			const SparseMatrix<double>& getProbabilities() const;

			// Returns probability/belief that follower is contained in obj's window
			double getProbability(ObjectID obj, ObjectID follower) const;
		
		private:
			CacheParameters params_;
			SparseMatrix<uint64_t> accessStats_;
			SparseMatrix<double> probabilities_;
			std::vector<ObjectID> history_;
			std::vector<uint64_t> columnSums_;
			std::vector<uint64_t> rowSums_;

			#ifdef BELIEFCACHE_CEPH
			CephContext* cct_;
			#endif
	};


} //namespace cache

#endif
