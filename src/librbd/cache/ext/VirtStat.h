#ifndef VIRT_STAT_H
#define VIRT_STAT_H

// This is a bit of a mess...
#ifndef PREDICTCACHE_SIM
#define PREDICTCACHE_CEPH
#endif

// STL includes
#include <string>
#include <cstdint>
#include <vector>

// Boost includes
#define BOOST_DISABLE_ASSERTS
#include <boost/numeric/ublas/matrix_sparse.hpp>

// Ceph includes
#ifdef PREDICTCACHE_CEPH
#include "librbd/ImageCtx.h"
#endif

// Project includes
#include "Element.h"
#include "Globals.h"

namespace predictcache {

	using ObjectID = uint64_t;

	template <typename T>
	using SparseMatrix = boost::numeric::ublas::mapped_matrix<T>;

	class VirtStat{
		public:
			#ifndef PREDICTCACHE_CEPH
			VirtStat();
			#else
			VirtStat(CacheParameters params, CephContext* cct);
			#endif
			
			/*
			void get_main_key(const std::string key, uint64_t & id);
			void get_index(const std::string key, uint64_t & id);
			std::string fill_hash(const uint64_t index);
			std::string hasher(const uint64_t obj, const uint64_t index);
			void update_rec(const uint64_t obj);
			void update_counts(const uint64_t obj, const uint64_t next_obj);
			*/

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

			#ifdef PREDICTCACHE_CEPH
			CephContext* cct_;
			#endif
	};


} //namespace cache

#endif
