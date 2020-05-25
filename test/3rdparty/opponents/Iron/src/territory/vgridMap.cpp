//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "vgridMap.h"
#include "../behavior/raiding.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class GridMapCell
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


GridMapCell::GridMapCell()
{
	fill(begin(myFreeUnitsAndBuildingsEach16frames), end(myFreeUnitsAndBuildingsEach16frames), 0);
	fill(begin(hisUnitsAndBuildingsEach16frames), end(hisUnitsAndBuildingsEach16frames), 0);
}

void GridMapCell::UpdateStats() const
{
	{
		int myFreeUnits = count_if(MyUnits.begin(), MyUnits.end(), [](const MyUnit * u)
							{
								if (!u->GetStronghold())
									if (!u->Is(Terran_Vulture_Spider_Mine))
										if (!(u->GetBehavior()->IsRaiding() && u->GetBehavior()->IsRaiding()->Waiting()))
											return true;

								return false;
							});
		int myFreeBuildings = count_if(MyBuildings.begin(), MyBuildings.end(), [](const MyBuilding * b){ return b->Completed() && !b->GetStronghold(); });
		int myFreeUnitsAndBuildings = myFreeUnits + myFreeBuildings;
	
		myFreeUnitsAndBuildingsLast256Frames -= myFreeUnitsAndBuildingsEach16frames[0];
		myFreeUnitsAndBuildingsLast256Frames += myFreeUnitsAndBuildings;

		for (int i = 1 ; i < 16 ; ++i)
			myFreeUnitsAndBuildingsEach16frames[i-1] = myFreeUnitsAndBuildingsEach16frames[i];

		myFreeUnitsAndBuildingsEach16frames[16-1] = myFreeUnitsAndBuildings;
	}

	{
		const int hisUnitsAndBuildings = HisUnits.size() + HisBuildings.size();
	
		hisUnitsAndBuildingsLast256Frames -= hisUnitsAndBuildingsEach16frames[0];
		hisUnitsAndBuildingsLast256Frames += hisUnitsAndBuildings;

		for (int i = 1 ; i < 16 ; ++i)
			hisUnitsAndBuildingsEach16frames[i-1] = hisUnitsAndBuildingsEach16frames[i];

		hisUnitsAndBuildingsEach16frames[16-1] = hisUnitsAndBuildings;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VGridMap
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


void VGridMap::UpdateLastFrameVisible(const MyBWAPIUnit * u, GridMapCell & Cell)
{
	TilePosition t(u->Pos());
	TilePosition d(t.x % cell_width_in_tiles, t.y % cell_width_in_tiles);
	int sightInTiles = u->Sight()/32 + 1;
	if ((d.x - sightInTiles < 0) && (d.x + sightInTiles > cell_width_in_tiles) &&
		(d.y - sightInTiles < 0) && (d.y + sightInTiles > cell_width_in_tiles))
		Cell.lastFrameVisible = ai()->Frame();
}


void VGridMap::Add(MyUnit * u)
{
//	ai()->SetDelay(5000);
///	bw << "VGridMap::Add " << u->NameWithId() << endl;

	GridMapCell & Cell = GetCell(TilePosition(u->Pos()));
	auto & List = Cell.MyUnits;
	PUSH_BACK_UNCONTAINED_ELEMENT(List, u);

	UpdateLastFrameVisible(u, Cell);
}


void VGridMap::Add(MyBuilding * b)
{
	GridMapCell & Cell = GetCell(TilePosition(b->Pos()));
	auto & List = Cell.MyBuildings;
	PUSH_BACK_UNCONTAINED_ELEMENT(List, b);

	UpdateLastFrameVisible(b, Cell);
}


void VGridMap::Add(HisUnit * u)
{
	auto & List = GetCell(TilePosition(u->Pos())).HisUnits;
	PUSH_BACK_UNCONTAINED_ELEMENT(List, u);
}


void VGridMap::Add(HisBuilding * b)
{
	auto & List = GetCell(TilePosition(b->Pos())).HisBuildings;
	PUSH_BACK_UNCONTAINED_ELEMENT(List, b);
}


void VGridMap::Remove(MyUnit * u)
{
///	bw << "VGridMap::Remove " << u->NameWithId() << endl;

	auto & List = GetCell(TilePosition(u->Pos())).MyUnits;
	auto i = find(List.begin(), List.end(), u);
	assert_throw(i != List.end());
	fast_erase(List, distance(List.begin(), i));
}


void VGridMap::Remove(MyBuilding * b)
{
	auto & List = GetCell(TilePosition(b->Pos())).MyBuildings;
	auto i = find(List.begin(), List.end(), b);
	assert_throw(i != List.end());
	fast_erase(List, distance(List.begin(), i));
}


void VGridMap::Remove(HisUnit * u)
{
	auto & List = GetCell(TilePosition(u->Pos())).HisUnits;
	auto i = find(List.begin(), List.end(), u);
	assert_throw(i != List.end());
	fast_erase(List, distance(List.begin(), i));
}


void VGridMap::Remove(HisBuilding * b)
{
	auto & List = GetCell(TilePosition(b->Pos())).HisBuildings;
	auto i = find(List.begin(), List.end(), b);
	assert_throw(i != List.end());
	fast_erase(List, distance(List.begin(), i));
}


void VGridMap::UpdateStats() const
{
	if (ai()->Frame() % 16 == 0)
		for (const GridMapCell & Cell : Cells())
			Cell.UpdateStats();
}



template<class T>
vector<T *> collect(const VGridMap & Grid, function<const vector<T *> * (const GridMapCell &)> f, TilePosition topLeft, TilePosition bottomRight)
{
	vector<T *> Res;

	int i1, j1, i2, j2;
	tie(i1, j1) = Grid.GetCellCoords(topLeft);
	tie(i2, j2) = Grid.GetCellCoords(bottomRight);

	for (int j = j1 ; j <= j2 ; ++j)
	for (int i = i1 ; i <= i2 ; ++i)
	{
		const GridMapCell & Cell = Grid.GetCell(i, j);
		for (const auto & e : *f(Cell))
			if (inBoundingBox(TilePosition(e->Pos()), topLeft, bottomRight))
				Res.push_back(e);
	}

	return Res;
}


vector<MyUnit *> VGridMap::GetMyUnits(TilePosition topLeft, TilePosition bottomRight) const
{
	return collect<MyUnit>(*this, [](const GridMapCell & Cell){ return &Cell.MyUnits; }, topLeft, bottomRight);
}


vector<MyBuilding *> VGridMap::GetMyBuildings(TilePosition topLeft, TilePosition bottomRight) const
{
	return collect<MyBuilding>(*this, [](const GridMapCell & Cell){ return &Cell.MyBuildings; }, topLeft, bottomRight);
}


vector<HisUnit *> VGridMap::GetHisUnits(TilePosition topLeft, TilePosition bottomRight) const
{
	return collect<HisUnit>(*this, [](const GridMapCell & Cell){ return &Cell.HisUnits; }, topLeft, bottomRight);
}


vector<HisBuilding *> VGridMap::GetHisBuildings(TilePosition topLeft, TilePosition bottomRight) const
{
	return collect<HisBuilding>(*this, [](const GridMapCell & Cell){ return &Cell.HisBuildings; }, topLeft, bottomRight);
}







/*

vector<BWAPI::Unit> SimpleGridMap::GetUnits(TilePosition topLeft, TilePosition bottomRight) const
{
	vector<BWAPI::Unit> Res;

	int i1, j1, i2, j2;
	tie(i1, j1) = GetCellCoords(topLeft);
	tie(i2, j2) = GetCellCoords(bottomRight);

	for (int j = j1 ; j <= j2 ; ++j)
	for (int i = i1 ; i <= i2 ; ++i)
		for (BWAPI::Unit unit : GetCell(i, j).Units)
			if (inBoundingBox(TilePosition(unit->getPosition()), topLeft, bottomRight))
				Res.push_back(unit);

	return Res;
}


void SimpleGridMap::Add(BWAPI::Unit unit)
{
	auto & List = GetCell(TilePosition(unit->getPosition())).Units;

	if (!contains(List, unit)) List.push_back(unit);
}


void SimpleGridMap::Remove(BWAPI::Unit unit)
{
	auto & List = GetCell(TilePosition(unit->getPosition())).Units;

	really_remove(List, unit);
}
*/
	
} // namespace iron



