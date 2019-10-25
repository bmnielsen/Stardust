#include "Base.h"

Base::Base(BWAPI::TilePosition _tile, const BWEM::Base *_bwemBase)
        : owner(nullptr)
        , resourceDepot(nullptr)
        , ownedSince(-1)
        , lastScouted(-1)
        , spiderMined(false)
        , requiresMineralWalkFromEnemyStartLocations(false)
        , tile(_tile)
        , bwemBase(_bwemBase)
{
    int x = 0;
    int y = 0;
    for (auto mineral : bwemBase->Minerals())
    {
        x += mineral->Unit()->getPosition().x;
        y += mineral->Unit()->getPosition().y;
    }

    mineralLineCenter = (getPosition() + BWAPI::Position(x / mineralPatchCount(), y / mineralPatchCount()) * 2) / 3;
}

std::vector<BWAPI::Unit> Base::mineralPatches() const
{
    std::vector<BWAPI::Unit> result;
    for (auto mineral : bwemBase->Minerals())
    {
        result.push_back(mineral->Unit());
    }

    return result;
}

std::vector<BWAPI::Unit> Base::geysers() const
{
    std::vector<BWAPI::Unit> result;
    for (auto geyser : bwemBase->Geysers())
    {
        if (geyser->Unit()->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;
        result.push_back(geyser->Unit());
    }

    return result;
}

std::vector<BWAPI::Unit> Base::refineries() const
{
    std::vector<BWAPI::Unit> result;
    for (auto geyser : bwemBase->Geysers())
    {
        if (geyser->Unit()->getPlayer() != BWAPI::Broodwar->self()) continue;
        result.push_back(geyser->Unit());
    }

    return result;
}

int Base::minerals() const
{
    int sum = 0;
    for (auto mineral : bwemBase->Minerals())
    {
        sum += mineral->Amount();
    }

    return sum;
}

int Base::gas() const
{
    int sum = 0;
    for (auto geyser : bwemBase->Geysers())
    {
        sum += geyser->Amount();
    }

    return sum;
}

bool Base::isStartingBase() const
{
    return std::find(
            BWAPI::Broodwar->getStartLocations().begin(),
            BWAPI::Broodwar->getStartLocations().end(),
            tile) != BWAPI::Broodwar->getStartLocations().end();
}
