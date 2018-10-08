#include "WorkloadRead.h"

// STL includes
#include <fstream>
#include <iostream>

std::vector<uint64_t> readWorkload(const std::string& path) {
    std::vector<uint64_t> result;
    std::string filename;

     for (uint64_t i = 1; i <= 9; i ++) {
        filename = path + std::to_string(i) + ".csv";
        std::ifstream file(filename);

        if (!file.is_open()) {
            std::cout << "Cannot open workload file \"" + filename + "\"\n";
            continue;
        }

        uint64_t accessId;
        
        while(file >> accessId) {
            result.push_back(accessId);
        }
    }

    return result;
}
