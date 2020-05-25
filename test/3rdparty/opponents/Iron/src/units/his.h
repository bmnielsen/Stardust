//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef HIS_H
#define HIS_H

#include <BWAPI.h>
#include "bwapiUnits.h"
#include <memory>


namespace iron
{

class Destroying;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class HisUnit : public HisBWAPIUnit
{
public:
									HisUnit(BWAPI::Unit u);
									~HisUnit();

	void							Update() override;

	const HisUnit *					IsHisUnit() const override			{ return this; }
	HisUnit *						IsHisUnit() override				{ return this; }

	MyUnit *						PursuingTarget() const				{CI(this); return m_pPursuingTarget; }
	frame_t							PursuingTargetLastFrame() const		{CI(this); return m_pPursuingTargetLastFrame; }
	void							SetPursuingTarget(MyUnit * pAgent);

	// The Destroying units that are targeting this.
	const vector<Destroying *> &	Destroyers() const					{CI(this); return m_Destroyers; }
	void							AddDestroyer(Destroying * pDestroyer);
	void							RemoveDestroyer(Destroying * pDestroyer);

	void							AddMineTargetingThis(Position pos)	{CI(this); m_minesTargetingThis.push_back(pos); }
	const vector<Position> &		MinesTargetingThis() const			{CI(this); return m_minesTargetingThis; }

	bool							WatchedOn() const					{ return m_watchedOn; }

	void							OnWorthScaning();
	bool							WorthScaning() const;

private:
	void							UpdateMineWatchedOn();

	vector<Position>				m_minesTargetingThis;
	bool							m_watchedOn;
	MyUnit *						m_pPursuingTarget = nullptr;
	frame_t							m_pPursuingTargetLastFrame = 0;
	frame_t							m_lastWorthScaning = 0;
	vector<Destroying *>			m_Destroyers;
};


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisBuilding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class HisBuilding : public HisBWAPIUnit
{
public:
	using BWAPIUnit::JustLifted;
	using BWAPIUnit::JustLanded;

									HisBuilding(BWAPI::Unit u);
									~HisBuilding();

	void							Update() override;

	const HisBuilding *				IsHisBuilding() const override	{ return this; }
	HisBuilding *					IsHisBuilding() override		{ return this; }

	TilePosition					Size() const					{CI(this); return m_size; }

private:
	bool							AtLeastOneTileIsVisible() const;

	const TilePosition				m_size;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisKnownUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class HisKnownUnit
{
public:
									HisKnownUnit(HisUnit * u);

	BWAPI::UnitType					Type() const				{CTHIS; return m_bwapiUnitType; }

	// - Either returns nullptr or returns u such that !u->InFog()
	// - nullptr means either the correspunding HisUnit is InFog() (i.e. some Terran_Siege_Tank_Siege_Mode)
	//   or there is no correspunding HisUnit but the BWAPI::Unit has not been destroyed.
	HisUnit *						GetHisUnit() const			{CTHIS; return m_pHisUnit; }

	frame_t							LastTimeVisible() const		{CTHIS; return m_lastTimeVisible; }
	Position						LastPosition() const		{CTHIS; return m_lastPosition; }
	bool							NoMoreHere() const			{CTHIS; return m_noMoreHere; }
	bool							NoMoreHere3Tiles() const	{CTHIS; return m_noMoreHere3; }
	int								LastLife() const			{CTHIS; return m_lastLife; }
	int								LastShields() const			{CTHIS; return m_lastShields; }

	frame_t							LastTimeChecked() const		{CTHIS; return m_lastTimeChecked; }
	frame_t							NextTimeToCheck() const		{CTHIS; return m_nextTimeToCheck; }
	bool							TimeToCheck() const;
	void							OnChecked();

	void							Update(HisUnit * u);

private:
	void							ResetCheckedInfo();
	void							UpdateNextTimeToCheck();

//									HisKnownUnit(const HisKnownUnit &) = delete;
	void							operator=(const HisKnownUnit &) = delete;

	BWAPI::UnitType					m_bwapiUnitType;
	HisUnit *						m_pHisUnit;
	frame_t							m_lastTimeVisible;
	Position						m_lastPosition;
	bool							m_noMoreHere;
	bool							m_noMoreHere3;
	int								m_lastLife;
	int								m_lastShields;

	frame_t							m_lastTimeChecked;
	frame_t							m_nextTimeToCheck;
	int								m_checkedCount;
};






} // namespace iron


#endif

