#include "MopUp.h"

#include "General.h"

MopUp::MopUp() : MainArmyPlay("MopUp"), squad(std::make_shared<MopUpSquad>())
{
    General::addSquad(squad);
}
