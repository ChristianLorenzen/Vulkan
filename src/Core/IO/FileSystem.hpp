#pragma once

#include <string>
#include <vector>

namespace Faye
{
    class FileSystem
    {
    public:
        static std::vector<char> readFile(const std::string &filename);
        static std::string readTextFile(const std::string &filename);
    };

}