//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef HIM_H
#define HIM_H

#include <BWAPI.h>
#include "his.h"
#include <memory>
#include <array>


namespace iron
{

class VBase;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Him
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Him
{
public:
											Him(char) {}

	void									Update();

	const BWAPI::Player						Player() const;
	BWAPI::Player							Player();

	VBase *									StartingVBase() const		{ return m_pStartingBase; }
	const Base *							StartingBase() const;
	VBase *									VNatural() const			{ return m_pNatural; }
	const Base *							Natural() const;
	const Area *							NaturalArea() const			{ return m_pNaturalArea; }

	BWAPI::Race								Race() const				{ return Player()->getRace(); }
	bool									IsTerran() const			{ return Race() == Races::Terran; }
	bool									IsProtoss() const			{ return Race() == Races::Protoss; }
	bool									IsZerg() const				{ return Race() == Races::Zerg; }

	bool									Accessible() const			{ return m_accessible; }
	void									SetNotAccessible()			{ m_accessible = false; }

	bool									StartingBaseDestroyed() const			{ return m_startingBaseDestroyed; }
	void									SetStartingBaseDestroyed()	{ m_startingBaseDestroyed = true; }

	void									AddUnit(BWAPI::Unit u);
	void									AddBuilding(BWAPI::Unit u);
	HisUnit *								FindUnit(BWAPI::Unit u) const;
	HisBuilding *							FindBuilding(BWAPI::Unit u) const;

	void									RemoveUnit(BWAPI::Unit u, bool informOthers);
	void									RemoveBuilding(BWAPI::Unit u, bool informOthers);

	void									OnShowUnit(BWAPI::Unit u);
	void									OnShowBuilding(BWAPI::Unit u);

	void									OnHideUnit(BWAPI::Unit u);
	void									OnHideBuilding(BWAPI::Unit u);
	void									OnDestroyed(BWAPI::Unit u);

	int										LostUnits(UnitType type) const		{ assert_throw(type <= max_utid_used); return m_lostUnits[type]; }
	int										CreationCount(UnitType type) const	{ assert_throw(type <= max_utid_used); return AllUnits(type).size() + m_lostUnits[type]; }

	Position								LastDragoonKilledPos() const		{ return m_lastDragoonKilledPos; }
	frame_t									LastDragoonKilledFrame() const		{ return m_lastDragoonKilledFrame; }


	// Access to his visible units and his tracked invisible units (marked InFog)
	const vector<unique_ptr<HisUnit>> &		Units() const				{ return m_Units; }
	vector<unique_ptr<HisUnit>> &			Units()						{ return m_Units; }
	const vector<HisUnit *> &				Units(UnitType type) const	{ assert_throw(type <= max_utid_used); assert_throw(!UnitType(type).isBuilding()); return m_UnitsByType[type]; }

	// Access to all his known units
	const map<Unit, HisKnownUnit> &			AllUnits() const				{ return m_KnownUnitMap; }
	const vector<HisKnownUnit *> &			AllUnits(UnitType type) const	{ assert_throw(type <= max_utid_used); assert_throw(!UnitType(type).isBuilding()); return m_KnownUnits[type]; }
	vector<HisKnownUnit *> &				AllUnits(UnitType type)			{ assert_throw(type <= max_utid_used); assert_throw(!UnitType(type).isBuilding()); return m_KnownUnits[type]; }

	// Access to his buildings
	const vector<unique_ptr<HisBuilding>> &	Buildings() const				{ return m_AllBuildings; }
	vector<unique_ptr<HisBuilding>> &		Buildings()						{ return m_AllBuildings; }
	const vector<HisBuilding *> &			Buildings(UnitType type) const	{ assert_throw(type <= max_utid_used); assert_throw(UnitType(type).isBuilding()); return m_Buildings[type]; }

	const Area *							GetArea() const;
	
	bool									HasBuildingsInNatural() const	{ return m_hasBuildingsInNatural; }
	
	bool									FoundBuildingsInMain() const	{ return m_foundBuildingsInMain; }
	void									SetFoundBuildingsInMain()		{ m_foundBuildingsInMain = true; }

	void									CheckTanksInFog();

	bool									MayDarkTemplar() const		{ return m_mayDarkTemplar; }
	void									SetMayDarkTemplar()			{ m_mayDarkTemplar = true; }

	bool									MayReaver() const			{ return m_mayReaver; }
	void									SetMayReaver()				{ m_mayReaver = true; }

	bool									MayShuttleOrObserver() const{ return m_mayShuttleOrObserver; }
	void									SetMayShuttleOrObserver()	{ m_mayShuttleOrObserver = true; }

	bool									MayDragoon() const			{ return m_mayDragoon; }
	void									SetMayDragoon()				{ m_mayDragoon = true; }

	bool									MayCarrier() const			{ return m_mayCarrier; }
	void									SetMayCarrier()				{ m_mayCarrier = true; }

	bool									MayWraith() const			{ return m_mayWraith; }
	void									SetMayWraith()				{ m_mayWraith = true; }

	bool									HasCannons() const			{ return m_hasCannons; }
	void									SetHasCannons()				{ m_hasCannons = true; }

	bool									MayHydraOrLurker() const	{ return m_mayHydraOrLurker; }
	void									SetMayHydraOrLurker()		{ m_mayHydraOrLurker = true; }

	bool									MayMuta() const				{ return m_mayMuta; }
	void									SetMayMuta()				{ m_mayMuta = true; }

	bool									SpeedLings() const			{ return m_speedLings; }

	bool									CanDetect(Position pos) const;
	bool									ZerglingPressure() const;
	bool									HydraPressure() const;
	bool									HydraPressure_needVultures() const;


private:
	VBase *									FindStartingBase() const;
	void									SetStartingBase(VBase * base)	{ assert_throw(!m_pStartingBase && base); m_pStartingBase = base; }
	void									UpdateStartingBase();
	void									CheckObsoleteInFogUnit(BWAPI::Unit u);
	void									CheckObsoleteInFogBuilding(BWAPI::Unit u);
	void									UpdateKnwownUnits();

	enum { max_utid_used = Protoss_Shield_Battery };

	vector<unique_ptr<HisUnit>>				m_Units;
	vector<HisUnit *>						m_UnitsByType[max_utid_used + 1];

	vector<unique_ptr<HisBuilding>>			m_AllBuildings;
	vector<HisBuilding *>					m_Buildings[max_utid_used + 1];

	VBase *									m_pStartingBase = nullptr;
	VBase *									m_pNatural = nullptr;
	const Area *							m_pNaturalArea = nullptr;

	bool									m_accessible = true;
	bool									m_startingBaseDestroyed = false;
	bool									m_hasBuildingsInNatural = false;
	bool									m_foundBuildingsInMain = false;
	bool									m_mayDarkTemplar = false;
	bool									m_mayReaver = false;
	bool									m_mayShuttleOrObserver = false;
	bool									m_mayDragoon = false;
	bool									m_mayCarrier = false;
	bool									m_mayWraith = false;
	bool									m_hasCannons = false;
	bool									m_mayHydraOrLurker = false;
	bool									m_mayMuta = false;
	bool									m_speedLings = false;

	map<Unit, HisKnownUnit>					m_KnownUnitMap;
	vector<HisKnownUnit *>					m_KnownUnits[max_utid_used + 1];

	array<int, max_utid_used + 1>			m_lostUnits = {};

	Position								m_lastDragoonKilledPos;
	frame_t									m_lastDragoonKilledFrame;
	mutable frame_t							m_lastZerglingPressureFrame = 0;
	mutable frame_t							m_lastHydraPressureFrame = 0;
	mutable frame_t							m_lastHydraPressure_needVulturesFrame = 0;
	mutable int								m_maxHydraAllTimes = 0;

};


class Him & him();

} // namespace iron


#endif

