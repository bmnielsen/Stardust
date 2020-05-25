//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef CONSTRUCTING_H
#define CONSTRUCTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class VBase;
class ConstructingExpert;
FORWARD_DECLARE_MY(Terran_SCV)

int beingConstructed(BWAPI::UnitType type);

bool rawCanBuild(BWAPI::UnitType type, TilePosition location);

MyUnit * findBlockingMine(BWAPI::UnitType type, TilePosition location);

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Constructing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//
//	Mineral::Data() is used to store the number of miners assigned to this Mineral.
//

class Constructing : public Behavior<My<Terran_SCV>>
{
public:
	static const vector<Constructing *> &	Instances()					{ return m_Instances; }

	static TilePosition			FindFirstDepotPlacement();
	static TilePosition			FindSecondDepotPlacement();
	static TilePosition			FindFirstBarracksPlacement();
	static TilePosition			FindFirstFactoryPlacement();
	static TilePosition			FindBuildingPlacement(BWAPI::UnitType type, VBase * base, TilePosition near, int maxTries = 1);

	enum state_t {reachingLocation, waitingForMoney, constructing};

								Constructing(My<Terran_SCV> * pSCV, BWAPI::UnitType type, ConstructingExpert * pExpert = nullptr);
								~Constructing();

	const Constructing *		IsConstructing() const override		{ return this; }
	Constructing *				IsConstructing() override			{ return this; }

	string						Name() const override				{ return "constructing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Yellow; }
	Text::Enum					GetTextColor() const override		{ return Text::Yellow; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * ) const override { return false; }

	state_t						State() const						{CI(this); return m_state; }

	VBase *						GetBase() const						{CI(this); return m_pBase; }

	BWAPI::UnitType				Type() const						{CI(this); return m_type; }
	TilePosition				Location() const					{CI(this); return m_location;; }
	Position					CenterLocation() const				{CI(this); return Position(Location()) + Position(Type().tileSize()) / 2; }

	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;

	void						ClearExpert()						{CI(this); m_pExpert = nullptr; }

private:
	static TilePosition			FindChokePointBunkerPlacement();

	void						ChangeState(state_t st)				{CI(this); assert_throw(m_state != st); m_state = st; OnStateChanged(); }
	void						OnFrame_reachingLocation();
	void						OnFrame_waitingForMoney();
	void						OnFrame_constructing();

	TilePosition				FindBuildingPlacement(TilePosition near) const;
	const Tile &				LocationTile() const;
	BWAPIUnit *					FindBuilding() const;

	BWAPI::UnitType				m_type;
	VBase *						m_pBase;
	state_t						m_state = reachingLocation;
	TilePosition				m_location = TilePositions::None;
	frame_t						m_inStateSince;
	mutable frame_t				m_timeToReachLocation = 200;
	ConstructingExpert *		m_pExpert;

	static vector<Constructing *>	m_Instances;
};







} // namespace iron


#endif

