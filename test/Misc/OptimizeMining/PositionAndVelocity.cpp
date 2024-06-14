#include "PositionAndVelocity.h"

#include <regex>

#include "CsvTools.h"

namespace
{
    const std::regex parser("\\(x=([0-9]+) y=([0-9]+) dx=([0-9]+) dy=([0-9]+) h=([0-9]+)\\)");
}

PositionAndVelocity PositionAndVelocity::fromString(const std::string &str)
{
    std::smatch matches;
    if (!std::regex_search(str, matches, parser) || matches.size() != 5)
    {
        throw std::runtime_error("malformed PositionAndVelocity string given");
    }

    return PositionAndVelocity{
        std::stoi(matches[0].str()),
        std::stoi(matches[1].str()),
        std::stoi(matches[2].str()),
        std::stoi(matches[3].str()),
        std::stoi(matches[4].str())};
}

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity)
{
    os << "(x=" << positionAndVelocity.x
       << " y=" << positionAndVelocity.y
       << " dx=" << positionAndVelocity.dx
       << " dy=" << positionAndVelocity.dy
       << " h=" << positionAndVelocity.heading
       << ")";

    return os;
}

std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec)
{
    CsvTools::outputList(os, vec);
    return os;
}

std::ostream &operator<<(std::ostream &os, const std::vector<BWAPI::Position> &vec)
{
    CsvTools::outputList(os, vec);
    return os;
}
