#pragma once

#include <vector>

namespace Faye {
    class File {
        public:
            static std::vector<char> readFile(const std::string& filename);
    };

}