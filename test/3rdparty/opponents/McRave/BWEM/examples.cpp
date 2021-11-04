//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "examples.h"
#include "map.h"
#include "base.h"
#include "neutral.h"
#include "gridMap.h"
#include "bwapiExt.h"

using namespace BWAPI;
using namespace BWAPI::UnitTypes::Enum;
namespace { auto& bw = Broodwar; }

using namespace std;


namespace BWEM {

using namespace utils;
using namespace BWAPI_ext;

namespace utils
{
    

struct SimplegridMapCell
{
    vector<Unit>        Units;
};



class SimplegridMap : public gridMap<SimplegridMapCell, 8>
{
public:
                        SimplegridMap(const Map * pMap) : gridMap(pMap) {}

    void                Add(Unit unit);
    void                Remove(Unit unit);

    vector<Unit>        GetUnits(TilePosition topLeft, TilePosition bottomRight, Player player) const;
};


void SimplegridMap::Add(Unit unit)
{
    auto& List = GetCell(TilePosition(unit->getPosition())).Units;

    if (!contains(List, unit)) List.push_back(unit);
}


void SimplegridMap::Remove(Unit unit)
{
    auto& List = GetCell(TilePosition(unit->getPosition())).Units;

    really_remove(List, unit);
}


vector<Unit> SimplegridMap::GetUnits(TilePosition topLeft, TilePosition bottomRight, Player player) const
{
    vector<Unit> Res;

    int i1, j1, i2, j2;
    tie(i1, j1) = GetCellCoords(topLeft);
    tie(i2, j2) = GetCellCoords(bottomRight);

    for (int j = j1 ; j <= j2 ; ++j)
    for (int i = i1 ; i <= i2 ; ++i)
        for (Unit unit : GetCell(i, j).Units)
            if (unit->getPlayer() == player)
                if (BWAPI_ext::inBoundingBox(TilePosition(unit->getPosition()), topLeft, bottomRight))
                    Res.push_back(unit);

    return Res;
}


void gridMapExample(const Map & theMap)
{

    // 1) Initialization
    SimplegridMap GridManager(&theMap);

    //  Note: generally, you will create one instance of gridMap, after calling Map::Instance().Initialize().
    
    
    // 2) Update (in AIModule::onFrame)
    for (int j = 0 ; j < GridManager.Height() ; ++j)
    for (int i = 0 ; i < GridManager.Width() ; ++i)
        GridManager.GetCell(i, j).Units.clear();

    for (Unit unit : Broodwar->getAllUnits())
        GridManager.Add(unit);

    //  Note: alternatively, you could use the Remove and Add methods only, in the relevant BWAPI::AIModule methods.


    // 3) Use
    TilePosition centerTile(theMap.Center());
    for (Unit unit : GridManager.GetUnits(centerTile-10, centerTile+10, Broodwar->self()))
        Broodwar << "My " << unit->getType().getName() << " #" << unit->getID() << " is near the center of the map." << endl;
}



}} // namespace BWEM::utils



