#pragma once

#include <BWAPI.h>

namespace CsvTools
{
    std::vector<std::string> readNextLine(std::istream &str, char sep = ';');

    template<typename T>
    std::vector<T> parseList(const std::string &str, char sep = ',')
    {
        std::vector<T> result;

        std::stringstream listStream(str);
        std::string item;

        while (std::getline(listStream, item, sep))
        {
            result.push_back(T::fromString(item));
        }

        return result;
    }

    template<typename T>
    void outputList(std::ostream &os, const std::vector<T> &list, char sep = ',')
    {
        std::string separator;
        for (const auto &item : list)
        {
            os << separator << item;
            separator = sep;
        }
    }
}
