#include "CsvTools.h"

std::vector<std::string> CsvTools::readNextLine(std::istream &str, char sep)
{
    std::vector<std::string> result;
    try
    {
        std::string line;
        std::getline(str, line);

        std::stringstream lineStream(line);
        std::string cell;

        while (std::getline(lineStream, cell, sep))
        {
            result.push_back(cell);
        }
    }
    catch (std::exception &ex)
    {
        std::cout << "Exception reading CSV line: " << ex.what() << std::endl;
    }
    return result;
}
