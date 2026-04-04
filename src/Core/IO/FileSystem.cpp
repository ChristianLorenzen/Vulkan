#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Core/IO/FileSystem.hpp"

using namespace Faye;

std::vector<char> Faye::FileSystem::readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

std::string Faye::FileSystem::readTextFile(const std::string &filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}