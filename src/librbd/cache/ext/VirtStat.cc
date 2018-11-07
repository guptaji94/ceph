// Include boost libraries first to avoid Ceph conflicts
#include <boost/numeric/ublas/matrix_proxy.hpp>

#include "VirtStat.h"

// STL includes
#include <algorithm>
#include <iostream>

// Project includes
#include "Globals.h"

using namespace boost::numeric::ublas;

namespace predictcache {
	//void VirtStat::get_main_key(const uint64_t key, uint64_t &id);

  VirtStat::VirtStat() :
    params_(vcacheGlobals),
    accessStats_(params_.max_matrix_size, params_.max_matrix_size),
    probabilities_(params_.max_matrix_size, params_.max_matrix_size),
    columnSums_(params_.max_matrix_size, 0), rowSums_(params_.max_matrix_size, 0)
  {
    // Construct VirtStat with two max_elem x max_elem sparse matrices
  }

  std::vector<Element> VirtStat::updateHistory(ObjectID obj, std::vector<Element>& cacheElements)
  {
    // If we don't have any other elements in the history, don't do anything! 
    if (history_.empty()) {
      history_.push_back(obj);     
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

    for (auto i = history_.begin() + windowStart; i != history_.end() - 1; i++) {
      updateCounts(*i, obj);
    }

    // Find belief values for element in the window n
    // n, obj  

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

    // Insert as Element
    for (auto i : cacheCandidates) {
      result.emplace_back(i.first, i.second);
    }

    // Return value optimizations should prevent duplications - check
    return result;
  }

  void VirtStat::setParameters(CacheParameters params) {
    params_ = params;
  }

  void VirtStat::findMaxBelief(std::vector<Element>& candidates,
    std::vector<ObjectID>::iterator votersStart,
    std::vector<ObjectID>::iterator votersEnd, double threshold)
  {
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
