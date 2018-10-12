// Include boost libraries first to avoid Ceph conflicts
#include <boost/numeric/ublas/matrix_proxy.hpp>

#include "VirtStat.h"

// STL includes
#include <algorithm>
#include <iostream>

// Project includes
#include "Globals.h"

// Provide optional debug messages via Ceph
#ifdef PREDICTCACHE_CEPH
#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "PredictCache::VirtStat: " << this << " " \
                           <<  __func__ << ": "
#define put_msg(z) (ldout(cct_, 20) << z << dendl)
#else
#define put_msg(z)
#endif

using namespace boost::numeric::ublas;

namespace predictcache {
	//void VirtStat::get_main_key(const uint64_t key, uint64_t &id);

  #ifndef PREDICTCACHE_CEPH
  VirtStat::VirtStat() :
    params_(vcacheGlobals),
    accessStats_(params_.max_matrix_size, params_.max_matrix_size),
    probabilities_(params_.max_matrix_size, params_.max_matrix_size),
    columnSums_(params_.max_matrix_size, 0), rowSums_(params_.max_matrix_size, 0)
  {
    // Construct VirtStat with two max_elem x max_elem sparse matrices
  }
  #else
  VirtStat::VirtStat(CacheParameters params, CephContext* cct) :
    params_(params),
    accessStats_(params.max_matrix_size, params.max_matrix_size),
    probabilities_(params.max_matrix_size, params.max_matrix_size),
    columnSums_(params.max_matrix_size, 0), rowSums_(params.max_matrix_size, 0),
    cct_(cct)
  {
    ldout(cct_, 20) <<"Loading VirtStat matrices of size " << params_.max_matrix_size << dendl;
  }
  #endif

  std::vector<Element> VirtStat::updateHistory(ObjectID obj, std::vector<Element>& cacheElements)
  {
    // If we don't have any other elements in the history, don't do anything!
    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<"Adding " << obj << " to virtual stat history"<< dendl;
    #endif    
    if (history_.empty()) {
      history_.push_back(obj);
      #ifdef PREDICTCACHE_CEPH
      ldout(cct_,20) <<"No items in virtual stat history"<< dendl;
      #endif      
      return std::vector<Element>();
    }

    uint64_t windowStart;

    // Make sure that we don't ever start the window before 0
    if (history_.size() < (params_.window_size + 1)) {
      windowStart = 0;
    } else {
      windowStart = history_.size() - (params_.window_size + 1);
    }

    // Append to history
    history_.push_back(obj);

    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<obj << " inserted into virtual stat history"<< dendl;
    #endif
    for (auto i = history_.begin() + windowStart; i != history_.end() - 1; i++) {
      updateCounts(*i, obj);
    }

    // Find belief values for element in the window n
    // n, obj
    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) << "Calculating significance values for the range " << windowStart 
		   << ", " << (history_.size() - 1) << dendl;
    #endif    

    uint64_t votersStart;

    if (history_.size() < params_.voter_size) {
      votersStart = 0;
    } else {
      votersStart = history_.size() - params_.voter_size;
    }

    calculateSignificance(obj);

    auto candidates = findCacheCandidates(obj, params_.cc_size);

    if (candidates.empty()) {
      return candidates;
    }

    std::copy(cacheElements.begin(), cacheElements.end(), std::back_inserter(candidates));
    calculateSignificance(history_.begin() + votersStart, history_.end() - 1,
      candidates.begin(), candidates.end());

    findMaxBelief(candidates, history_.begin() + votersStart, history_.end(), params_.thresh);

    return candidates;
  }

  // Increments the access counts for next after obj
  void VirtStat::updateCounts(ObjectID obj, ObjectID next) {
    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<"Inserting element " << obj << ", " << next << " to access matrix"<< dendl;
    #endif
    accessStats_(obj, next) += 1;
    rowSums_[obj] += 1;
    columnSums_[next] += 1;
  }

  void VirtStat::calculateSignificance(ObjectID start)
  {
    // Calculate probabilities and belief values from access history

    // Get row and column corresponding to the first object
    matrix_row<SparseMatrix<uint64_t>> objRow(accessStats_, start);
    double rowSum = static_cast<double>(rowSums_[start]);


    if (rowSum > (params_.minimum_history_to_consider * params_.window_size)) {
      for (auto i = objRow.begin(); i != objRow.end(); i++) {
        if (*i != 0) {
          #ifdef PREDICTCACHE_CEPH
          ldout(cct_,20) <<"Calculating probability for " << start << ", " << i.index() << dendl;
          #endif

          uint64_t col = i.index();
          double oFrequency = static_cast<double>(columnSums_[start]);
          double xFrequency = static_cast<double>(columnSums_[col]);

          if (oFrequency != 0.0) {
            probabilities_(start, col) = (*i * xFrequency) / (rowSum * oFrequency);
          }
        }
      }
    }
  }

  void VirtStat::calculateSignificance(std::vector<ObjectID>::iterator votersStart,
    std::vector<ObjectID>::iterator votersEnd,
    std::vector<Element>::iterator candidatesStart,
    std::vector<Element>::iterator candidatesEnd)
  {
    for (auto i = votersStart; i != votersEnd; i++) {
      double rowSum = static_cast<double>(rowSums_[*i]);
      double oFrequency = static_cast<double>(columnSums_[*i]);

      if (rowSum > (params_.minimum_history_to_consider * params_.window_size) &&
        oFrequency != 0)
      {
        for (auto j = candidatesStart; j != candidatesEnd; j++) {
          double xFrequency = static_cast<double>(columnSums_[j->id]);

          probabilities_(*i, j->id) = (accessStats_(*i, j->id) * xFrequency) / (rowSum * oFrequency);
        }
      }
    }
  }

  std::vector<Element> VirtStat::findCacheCandidates(const ObjectID obj, uint64_t numCandidates)
  {
    // Allocate a vector of objects and probabilities with initial size equal to numCandidates
    std::vector<std::pair<ObjectID, double>> cacheCandidates;
    matrix_row<SparseMatrix<double>> objProbabilities(probabilities_, obj);

    std::vector<Element> result;

    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<"Finding cache candidates for item " << obj<< dendl;
    #endif    
    for (auto i = objProbabilities.begin(); i != objProbabilities.end(); i++) {
      // Skip elements with a probability of 0
      if (*i == 0.0) {
        continue;
      }

      cacheCandidates.emplace_back(i.index(), *i);
    }

    // Sort id/probability pairs using STL sort on probabilites
    std::sort(cacheCandidates.begin(), cacheCandidates.end(),
      [](std::pair<ObjectID, double> a, std::pair<ObjectID, double> b) {
        return a.second > b.second;
      }
    );

    // Erase all but first numCandidates candidates
    if (cacheCandidates.size() > numCandidates) {
      cacheCandidates.erase(cacheCandidates.begin() + numCandidates, cacheCandidates.end());
    }

    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<"Found " << cacheCandidates.size() << " cache candidates for element "
      << obj << dendl;
    #endif
    // Insert as Element
    for (auto i : cacheCandidates) {
      result.emplace_back(i.first, i.second);
    }

    // Return value optimizations should prevent duplications - check
    return result;
  }

  void VirtStat::findMaxBelief(std::vector<Element>& candidates,
    std::vector<ObjectID>::iterator votersStart,
    std::vector<ObjectID>::iterator votersEnd, double threshold)
  {
    #ifdef PREDICTCACHE_CEPH
    ldout(cct_,20) <<"Calculating maximal belief values from voters"<< dendl;
    #endif
    for (auto i = candidates.begin(); i != candidates.end(); ) {
      // Find the max belief for each candidate by traversing the list of voters
      for (auto j = votersStart; j != votersEnd; j++) {
        double voterProb = probabilities_(*j, i->id);
        if (voterProb > i->belief) {
          i->belief = voterProb;
        }
      }

      // If i's belief is less than the threshold, remove it
      if (i->belief <= threshold) {
        i = candidates.erase(i);
      } else {
        i++;
      }
    }
  }

  const SparseMatrix<uint64_t>& VirtStat::getAccessStats() const {
    return accessStats_;
  }

  const SparseMatrix<double>& VirtStat::getProbabilities() const {
    return probabilities_;
  }

  double VirtStat::getProbability(ObjectID obj, ObjectID follower) const {
    return probabilities_(obj, follower);
  }
}
