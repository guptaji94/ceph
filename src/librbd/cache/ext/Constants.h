#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <string>
#include <cstdint>
// Class handling default values for different parameters

struct Constants {

  const uint64_t RCACHE_SIZE = 655360;
  const uint64_t VCACHE_SIZE = 1000;
  const uint64_t CC_SIZE = 10;
  const std::string MOST = "FULLL_HIST";
  const double THRESH = 0.12;
  const uint64_t VOTER_SIZE = 5;
  const uint64_t WINDOWS_SIZE = 5;

};

#endif //CONSTANTS_H
