//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VBASE_H
#define VBASE_H

#include <BWAPI.h>
#include "../utils.h"
#include "../defs.h"
#include <array>


namespace iron
{

class Stronghold;
class MyUnit;
class MyBuilding;
class Mining;
class Refining;
class Supplementing;
class VArea;
class Wall;

FORWARD_DECLARE_MY(Terran_Command_Center)
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VBase
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VBase
{
public:
	static VBase *					Get(const BWEM::Base * pBWEMPart);

									VBase(const BWEM::Base * pBWEMPart);
									~VBase();

	void							Initialize();

	const BWEM::Base *				BWEMPart() const				{ return m_pBWEMPart; }

	frame_t							CreationTime() const			{ return m_creationTime; }
	void							SetCreationTime();

	bool							Active() const;
	bool							Lost() const					{ return m_lost; }
	frame_t							LostTime() const				{ return m_LostTime; }

	bool							NeverBuiltYet() const			{ return !GetCC() && !Lost(); }
	bool							ShouldRebuild() const;

	const VArea *					GetArea() const;

	Stronghold *					GetStronghold() const			{ return m_pStronghold; }
	void							SetStronghold(Stronghold * sh)	{ assert_throw(sh && !m_pStronghold); m_pStronghold = sh; }

	int								MineralsAmount() const;
	int								GasAmount() const;

	const vector<Mining *> &		Miners() const					{ return m_Miners; }
	int								MaxMiners() const;
	int								LackingMiners() const			{ return MaxMiners() - Miners().size(); }
	void							AddMiner(Mining * pMiner);
	void							RemoveMiner(Mining * pMiner);

	const vector<Refining *> &		Refiners() const				{ return m_Refiners; }
	int								MaxRefiners() const;
	int								LackingRefiners() const			{ return MaxRefiners() - Refiners().size(); }
	void							AddRefiner(Refining * pRefiner);
	void							RemoveRefiner(Refining * pRefiner);


	const vector<Supplementing *> &	Supplementers() const					{ return m_Supplementers; }
	int								MaxSupplementers() const				{ return m_creationTime == 0 ? 3 : 1; }
	int								LackingSupplementers() const			{ return MaxSupplementers() - Supplementers().size(); }
	void							AddSupplementer(Supplementing * pSupplementer);
	void							RemoveSupplementer(Supplementing * pSupplementer);

	int								OtherSCVs() const;

	bool							CheckSupplementerAssignment();

	void							OnUnitIn(MyUnit * u);
	void							OnBuildingIn(MyBuilding * b);

	void							OnUnitOut(MyUnit * u);
	void							OnBuildingOut(MyBuilding * b);

	My<Terran_Command_Center> *		GetCC() const					{ return m_pCC; }
	void							SetCC(My<Terran_Command_Center> * pCC);

	Mineral *						FindVisibleMineral() const;

	VBase &							operator=(const VBase &) = delete;

	void							UpdateLastTimePatrolled() const;
	frame_t							LastTimePatrolled() const		{ return m_lastTimePatrolled; }

	bool							BlockedByOneOfMyMines() const;
	
	Position						PosBetweenMineralAndCC(const Mineral * m) const;

	pair<TilePosition, TilePosition>GuardingCorners() const		{ return m_GuardingCorners; }

	const array<TilePosition, 2>	FirstTurretsLocations() const	{ return m_firstTurretsLocations; }

	void							AssignWall();
	Wall *							GetWall() const					{ return m_pWall; }

private:
	void							ComputeGuardingCorners();
	void							ComputeFirstTurretLocation();

	const BWEM::Base *				m_pBWEMPart;
	frame_t							m_creationTime = 0;
	Stronghold *					m_pStronghold = nullptr;
	My<Terran_Command_Center> *		m_pCC = nullptr;
	vector<Mining *>				m_Miners;
	vector<Refining *>				m_Refiners;
	vector<Supplementing *>			m_Supplementers;
	mutable frame_t					m_lastTimePatrolled = 0;
	bool							m_lost = false;
	frame_t							m_LostTime;
	pair<TilePosition, TilePosition>m_GuardingCorners;
	array<TilePosition, 2>			m_firstTurretsLocations;
	Wall *							m_pWall;
};


inline VBase * ext(const BWEM::Base * base)
{
	return VBase::Get(base);
}


Position nearestEmpty(Position pos);



My<Terran_SCV> * findFreeWorker(const VBase * base,
								function<bool(const My<Terran_SCV> *)> pred = [](const My<Terran_SCV> *){ return true; },
								bool canTakeBlocking = false,
								bool urgent = false);

inline My<Terran_SCV> * findFreeWorker_urgent(const VBase * base,
								bool canTakeBlocking = false)
{
	return findFreeWorker(base,
						[](const My<Terran_SCV> *){ return true; },
						canTakeBlocking = false,
						true);
}



} // namespace iron


#endif

