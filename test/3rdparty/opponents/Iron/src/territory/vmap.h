//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VMAP_H
#define VMAP_H

#include <BWAPI.h>
#include "vbase.h"
#include "varea.h"
#include "vcp.h"
#include "../utils.h"


namespace iron
{

class BWAPIUnit;
class HisBuilding;


int pathExists(const Position & a, const Position &  b);

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VMap
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VMap
{
public:
									VMap(BWEM::Map * pMap);

	void							Initialize();
	void							Update();

	const BWEM::Map *				GetMap() const		{ return m_pMap; }

	const vector<VBase> &			Bases() const		{ return m_Bases; }
	vector<VBase> &					Bases()				{ return m_Bases; }
	VBase *							GetBaseAt(TilePosition location);

	const vector<VArea> &			Areas() const		{ return m_Areas; }
	vector<VArea> &					Areas()				{ return m_Areas; }

	const vector<VChokePoint> &		ChokePoints() const	{ return m_ChokePoints; }
	vector<VChokePoint> &			ChokePoints()		{ return m_ChokePoints; }

	BWAPIUnit *						GetBuildingOn(const Tile & tile) const			{ return static_cast<BWAPIUnit *>(tile.Ptr()); }
	void							SetBuildingOn(const Tile & tile, BWAPIUnit * b) { assert_throw(!b != !tile.Ptr()); tile.SetPtr(b); }

	bool							ImpasseOrNearBuildingOrNeutral(const Tile & tile) const	{ return (tile.Data() & 7) != 0; }
	bool							NearBuildingOrNeutral(const Tile & tile) const	{ return (tile.Data() & 3) != 0; }
	
	void							SetNearBuilding(const Tile & tile)				{ tile.SetData(tile.Data() | 1); }
	void							UnsetNearBuilding(const Tile & tile)			{ tile.SetData(tile.Data() & ~1); }

	void							SetNearNeutral(const Tile & tile)				{ tile.SetData(tile.Data() | 2); }
	void							UnsetNearNeutral(const Tile & tile)				{ tile.SetData(tile.Data() & ~2); }

	bool							Impasse(int i, const Tile & tile) const;
	void							SetImpasse(int i, const Tile & tile);
	void							UnsetImpasse(int i, const Tile & tile);

	bool							CommandCenterImpossible(const Tile & tile) const{ return (tile.Data() & 64) != 0; }
	void							SetCommandCenterImpossible(const Tile & tile)	{ tile.SetData(tile.Data() | 64); }

	bool							CommandCenterWithAddonRoom(const Tile & tile) const	{ return (tile.Data() & 128) != 0; }
	void							SetCommandCenterWithAddonRoom(const Tile & tile)	{ tile.SetData(tile.Data() | 128); }

	bool							FirstTurretsRoom(const Tile & tile) const		{ return (tile.Data() & 256) != 0; }
	void							SetFirstTurretsRoom(const Tile & tile)			{ tile.SetData(tile.Data() | 256); }

	bool							SCVTraffic(const Tile & tile) const				{ return (tile.Data() & 512) != 0; }
	void							SetSCVTraffic(const Tile & tile)				{ tile.SetData(tile.Data() | 512); }

	bool							InsideWall(const Tile & tile) const				{ return (tile.Data() & 1024) != 0; }
	void							SetInsideWall(const Tile & tile)				{ tile.SetData(tile.Data() | 1024); }

	bool							OutsideWall(const Tile & tile) const			{ return (tile.Data() & 2048) != 0; }
	void							SetOutsideWall(const Tile & tile)				{ tile.SetData(tile.Data() | 2048); }

	void							SetAddonRoom(const Tile & tile)					{ tile.SetData(tile.Data() | 16); }
	void							UnsetAddonRoom(const Tile & tile)				{ tile.SetData(tile.Data() & ~16); }
	bool							AddonRoom(const Tile & tile) const				{ return (tile.Data() & 16) != 0; }

	void							PutMine(const Tile & tile)						{ tile.SetData(tile.Data() | 32); }
	void							RemoveMine(const Tile & tile)					{ tile.SetData(tile.Data() & ~32); }
	bool							IsThereMine(const Tile & tile) const			{ return (tile.Data() & 32) != 0; }

	WalkPosition					GetIncreasingAltitudeDirection(WalkPosition pos) const;

	Position						RandomPosition(Position center, int radius) const;
	Position						RandomSeaPosition() const;

	int								BuildingsAndNeutralsAround(TilePosition center, int radius) const;
	HisBuilding *					EnemyBuildingsAround(TilePosition center, int radius) const;

	void							PrintAreaChains() const;
	void							PrintEnlargedAreas() const;
	void							PrintImpasses() const;

	VMap &							operator=(const VMap &) = delete;

	const CPPath &					MainPath() const								{ return m_MainPath; }
	int								MainPathLength() const							{ return m_MainPathLength; }

private:
	template<unsigned n> bool		Impasse(const Tile & tile) const				{ return (tile.Data() & n) != 0; }
	template<unsigned n> void		SetImpasse(const Tile & tile)					{ tile.SetData(tile.Data() | n); }
	template<unsigned n> void		UnsetImpasse(const Tile & tile)					{ tile.SetData(tile.Data() & ~n); }

	void							ComputeImpasses(int kind);
	void							ComputeImpasses_FillFromChokePoint(int kind, const Area & area, const ChokePoint * cp);
	void							ComputeImpasses_MarkRessources();

	BWEM::Map *						m_pMap;
	vector<VBase>					m_Bases;
	vector<VArea>					m_Areas;
	vector<VChokePoint>				m_ChokePoints;

	CPPath							m_MainPath;
	int								m_MainPathLength = 0;
};



} // namespace iron


#endif

