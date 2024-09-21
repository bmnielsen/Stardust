#include "CsvTools.h"

std::vector<std::string> CsvTools::readNextLine(std::istream &str, char sep)
{
    std::vector<std::string> result;
    std::string line;
    std::getline(str, line);

    std::stringstream lineStream(line);
    std::string cell;

    while (std::getline(lineStream, cell, sep))
    {
        result.push_back(cell);
    }

    return result;
}

std::vector<std::string> CsvTools::tokenizeList(const std::string &str, char sep)
{
    std::vector<std::string> result;

    std::stringstream listStream(str);
    std::string item;

    while (std::getline(listStream, item, sep))
    {
        result.push_back(item);
    }

    return result;
}
