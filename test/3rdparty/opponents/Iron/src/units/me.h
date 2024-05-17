//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef ME_H
#define ME_H

#include <BWAPI.h>
#include "my.h"
#include "../territory/vbase.h"
#include <memory>
#include <array>


namespace iron
{

class Stronghold;
class Production;
class Army;
class Wall;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Me
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Me
{
public:
											Me(char);
											~Me();

	void									Initialize();

	void									Update();
	void									RunExperts();
	void									RunBehaviors();
	void									RunProduction();

	const BWAPI::Player						Player() const;
	BWAPI::Player							Player();

	BWAPI::Race								Race() const							{ return Player()->getRace(); }

	bool									HasResearched(TechType type) const		{ return Player()->hasResearched(type); }
	bool									HasUpgraded(UpgradeType type) const		{ return Player()->getUpgradeLevel(type) == Player()->getMaxUpgradeLevel(type); }	// TODO: replace
	bool									TechAvailableIn(TechType techType, frame_t time) const;
	bool									TechAvailableIn(UpgradeType upgradeType, frame_t time) const;

	const iron::Production &				Production() const						{ return *m_Production.get(); }
	iron::Production &						Production()							{ return *m_Production.get(); }

	const iron::Army &						Army() const							{ return *m_Army.get(); }
	iron::Army &							Army()									{ return *m_Army.get(); }

	int										CreationCount(UnitType type) const		{ assert_throw(type <= max_utid_used); return m_creationCount[type]; }

	const vector<unique_ptr<MyUnit>> &		Units(UnitType type) const				{ assert_throw(type <= max_utid_used); assert_throw(!UnitType(type).isBuilding()); return m_Units[type]; }
	vector<unique_ptr<MyUnit>> &			Units(UnitType type)					{ assert_throw(type <= max_utid_used); assert_throw(!UnitType(type).isBuilding()); return m_Units[type]; }
	const vector<MyUnit *> &				Units() const							{ return m_AllUnits; }
	int										CompletedUnits(UnitType type) const;
	int										UnitsBeingTrained(UnitType type) const;
	void									AddUnit(BWAPI::Unit u);
	void									RemoveUnit(BWAPI::Unit u);
	void									SetSiegeModeType(BWAPI::Unit u);
	void									SetTankModeType(BWAPI::Unit u);

	const vector<unique_ptr<MyBuilding>> &	Buildings(UnitType type) const			{ assert_throw(type <= max_utid_used); assert_throw(UnitType(type).isBuilding()); return m_Buildings[type]; }
	vector<unique_ptr<MyBuilding>> &		Buildings(UnitType type)				{ assert_throw_plus(type <= max_utid_used, type.toString()); assert_throw(UnitType(type).isBuilding()); return m_Buildings[type]; }
	const vector<MyBuilding *> &			Buildings() const						{ return m_AllBuildings; }
	int										CompletedBuildings(UnitType type) const;
	int										BuildingsBeingTrained(UnitType type) const;
	void									AddBuilding(BWAPI::Unit u);
	void									RemoveBuilding(BWAPI::Unit u, check_t checkMode = check_t::check);
	MyBuilding *							FindBuildingNeedingBuilder(UnitType type, bool inStrongHold = true) const;

	int										LostUnitsOrBuildings(UnitType type) const		{ assert_throw(type <= max_utid_used); return m_lostUnits[type]; }

	const vector<VBase *> &					Bases() const							{ return m_Bases; }
	vector<VBase *> &						Bases()									{ return m_Bases; }
	void									AddBase(VBase * pBase);
	void									RemoveBase(VBase * pBase);
	const VBase *							GetVBase(int n) const					{ assert_throw(0 <= n && n < (int)m_Bases.size()); return m_Bases[n]; }
	VBase *									GetVBase(int n)							{ assert_throw(0 <= n && n < (int)m_Bases.size()); return m_Bases[n]; }
	const Base *							GetBase(int n) const					{ assert_throw(0 <= n && n < (int)m_Bases.size()); return m_Bases[n]->BWEMPart(); }
	const VBase *							StartingVBase() const					{ return GetVBase(0); }
	VBase *									StartingVBase()							{ return GetVBase(0); }
	const Base *							StartingBase() const					{ return GetBase(0); }

	vector<const Area *>					EnlargedAreas() const;

	const vector<unique_ptr<Stronghold>> &	StrongHolds() const						{ return m_Strongholds; }
	vector<unique_ptr<Stronghold>> &		StrongHolds()							{ return m_Strongholds; }
	void									AddBaseStronghold(VBase * b);

	int										MineralsAvailable() const				{ return m_mineralsAvailable; }
	int										GasAvailable() const					{ return m_gasAvailable; }
	int										SupplyUsed() const						{ return m_supplyUsed; }
	int										SupplyMax() const						{ return m_supplyMax; }
	int										SupplyAvailable() const					{ return m_supplyMax - m_supplyUsed; }

	int										FactoryActivity() const					{ return m_factoryActivity; }
	int										BarrackActivity() const					{ return m_barrackActivity; }

	double									MedicForce() const						{ return m_medicForce; }

	int										SCVworkers() const						{ return m_SCVworkers; }
	int										SCVsoldiers() const						{ return m_SCVsoldiers; }
	int										SCVsoldiersForever() const				{ return m_SCVsoldiersForever; }
	int										LackingSCVsoldiers() const				{ return m_SCVsoldiersObjective - m_SCVsoldiers; }
	int										ExceedingSCVsoldiers() const			{ return -LackingSCVsoldiers(); }

	int										TotalAvailableScans() const				{ return m_totalAvailableScans; }
	My<Terran_Comsat_Station> *				BestComsat() const						{ return m_pBestComsat; }

	bool									CanPay(const Cost & cost) const;

	void									OnBWAPIUnitDestroyed_InformOthers(BWAPIUnit * pBWAPIUnit);

	const Area *							GetArea() const							{ return GetBase(0)->GetArea(); }
	VArea *									GetVArea();

	int										CountMechGroundFighters() const;
private:
	void									UpdateScans();

	enum { max_utid_used = Terran_Bunker };

	vector<VBase *>							m_Bases;
	vector<unique_ptr<Stronghold>>			m_Strongholds;

	unique_ptr<iron::Production>			m_Production;
	unique_ptr<iron::Army>					m_Army;

	vector<unique_ptr<MyUnit>>				m_Units[max_utid_used + 1];
	vector<MyUnit *>						m_AllUnits;

	vector<unique_ptr<MyBuilding>>			m_Buildings[max_utid_used + 1];
	vector<MyBuilding *>					m_AllBuildings;

	array<int, max_utid_used + 1>			m_creationCount = {};
	array<int, max_utid_used + 1>			m_lostUnits = {};

	int										m_mineralsAvailable;
	int										m_gasAvailable;
	int										m_supplyUsed;
	int										m_supplyMax;

	int										m_factoryActivity;
	int										m_barrackActivity;
	double									m_medicForce;

	int										m_SCVworkers;
	int										m_SCVsoldiers;
	int										m_SCVsoldiersForever;
	int										m_SCVsoldiersObjective;
	int										m_freeTurrets;

	int										m_totalAvailableScans = 0;
	My<Terran_Comsat_Station> *				m_pBestComsat = nullptr;
};


class Me & me();


} // namespace iron


#endif

