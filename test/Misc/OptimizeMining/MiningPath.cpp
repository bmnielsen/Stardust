#include "MiningPath.h"

std::ostream &operator<<(std::ostream &os, const std::vector<BWAPI::Position> &vec)
{
    CsvTools::outputList(os, vec);
    return os;
}
