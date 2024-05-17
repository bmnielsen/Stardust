//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef GRID_MAP_H
#define GRID_MAP_H

#include <BWAPI.h>
#include "../utils.h"
#include "../defs.h"


namespace iron
{

class BWAPIUnit;
class MyUnit;
class MyBuilding;
class HisUnit;
class HisBuilding;
class MyBWAPIUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class GridMapCell
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


struct GridMapCell
{
public:
							GridMapCell();

	vector<MyUnit *>		MyUnits;
	vector<MyBuilding *>	MyBuildings;
	vector<HisUnit *>		HisUnits;
	vector<HisBuilding *>	HisBuildings;

	frame_t					lastFrameVisible = 0;

	void					UpdateStats() const;

	mutable int				myFreeUnitsAndBuildingsEach16frames[16];
	mutable int				myFreeUnitsAndBuildingsLast256Frames = 0;
	int						AvgMyFreeUnitsAndBuildingsLast256Frames() const	{ return (myFreeUnitsAndBuildingsLast256Frames+8) / 16; }

	mutable int				hisUnitsAndBuildingsEach16frames[16];
	mutable int				hisUnitsAndBuildingsLast256Frames = 0;
	int						AvgHisUnitsAndBuildingsLast256Frames() const	{ return (hisUnitsAndBuildingsLast256Frames+8) / 16; }
};


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VGridMap
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VGridMap : public GridMap<GridMapCell, 8>
{
public:
								VGridMap(Map * pMap) : GridMap(pMap) {}

	void						Add(MyUnit * u);
	void						Add(MyBuilding * b);
	void						Add(HisUnit * u);
	void						Add(HisBuilding * b);

	void						Remove(MyUnit * u);
	void						Remove(MyBuilding * b);
	void						Remove(HisUnit * u);
	void						Remove(HisBuilding * b);

	void						UpdateStats() const;

	vector<MyUnit *>			GetMyUnits(TilePosition topLeft, TilePosition bottomRight) const;
	vector<MyBuilding *>		GetMyBuildings(TilePosition topLeft, TilePosition bottomRight) const;
	vector<HisUnit *>			GetHisUnits(TilePosition topLeft, TilePosition bottomRight) const;
	vector<HisBuilding *>		GetHisBuildings(TilePosition topLeft, TilePosition bottomRight) const;

private:
	void						UpdateLastFrameVisible(const MyBWAPIUnit * u, GridMapCell & Cell);
};




/*
struct SimpleGridMapCell
{
	vector<BWAPI::Unit>		Units;
};



class SimpleGridMap : public GridMap<SimpleGridMapCell, 8>
{
public:
								SimpleGridMap(Map * pMap) : GridMap(pMap) {}

	void						Add(BWAPI::Unit unit);

	void						Remove(BWAPI::Unit unit);

	vector<BWAPI::Unit>			GetMyUnits(TilePosition topLeft, TilePosition bottomRight) const;
	vector<BWAPI::Unit>			GetHisUnits(TilePosition topLeft, TilePosition bottomRight) const;

private:
	vector<BWAPI::Unit>			GetUnits(TilePosition topLeft, TilePosition bottomRight) const;
};
*/

} // namespace iron


#endif

