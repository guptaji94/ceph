#ifndef ELEMENT_H
#define ELEMENT_H
#include <vector>
#include <cstdint>

namespace predictcache {
  struct Element {
    Element(uint64_t id, double belief) :
      id(id), belief(belief)
    {}

    uint64_t id;
    double belief;
    uint64_t hits = 0;
    bool pinned = false;
  };
}

#endif
