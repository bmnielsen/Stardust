#include "ProductionGoal.h"

std::ostream &operator<<(std::ostream &os, const ProductionGoal &goal)
{
    std::visit([&os](auto &&arg)
               {
                   os << arg;
               }, goal);
    return os;
}
