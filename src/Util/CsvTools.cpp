#include "CsvTools.h"

std::vector<std::string> CsvTools::readNextLine(std::istream &str, char sep)
{
    std::vector<std::string> outcome;
    std::string line;
    std::getline(str, line);

    std::stringstream lineStream(line);
    std::string cell;

    while (std::getline(lineStream, cell, sep))
    {
        outcome.push_back(cell);
    }

    return outcome;
}

std::vector<std::string> CsvTools::tokenizeList(const std::string &str, char sep)
{
    std::vector<std::string> outcome;

    std::stringstream listStream(str);
    std::string goalEntry;

    while (std::getline(listStream, goalEntry, sep))
    {
        outcome.push_back(goalEntry);
    }

    return outcome;
}
