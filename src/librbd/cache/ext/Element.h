#ifndef ELEMENT_H
#define ELEMENT_H
#include <vector>
#include <cstdint>
#include <mutex>
#include <list>

namespace predictcache {
  template <typename T>
	struct MutexVector {
		std::vector<T> elements;
		// std::mutex mutex;
	};

  template <typename T>
	struct MutexList {
		std::list<T> element;
		std::mutex mutex;
	};	
  
  struct Element {
    Element(uint64_t id, double belief = 0.0) :
      id(id), belief(belief)
    {}

    uint64_t id;
    double belief;
    uint64_t hits = 0;
    bool pinned = false;
  };
}

#endif
