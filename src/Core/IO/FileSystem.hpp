#pragma once

#include <vector>

namespace Faye
{
    class FileSystem
    {
    public:
        static std::vector<char> readFile(const std::string &filename);
    };

}