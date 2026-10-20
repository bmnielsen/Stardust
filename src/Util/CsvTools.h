#pragma once

#include <BWAPI.h>

namespace CsvTools
{
    std::vector<std::string> readNextLine(std::istream &str, char sep = ';');

    std::vector<std::string> tokenizeList(const std::string &str, char sep = ',');

    template<typename T>
    std::vector<T> parseList(const std::string &str, char sep = ',')
    {
        std::vector<T> outcome;

        std::stringstream listStream(str);
        std::string goalEntry;

        while (std::getline(listStream, goalEntry, sep))
        {
            outcome.push_back(T::fromString(goalEntry));
        }

        return outcome;
    }

    template<typename T>
    void outputList(std::ostream &os, const std::vector<T> &list, char sep = ',')
    {
        std::string separator;
        for (const auto &goalEntry : list)
        {
            os << separator << goalEntry;
            separator = sep;
        }
    }
}
