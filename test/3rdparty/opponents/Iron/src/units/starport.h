//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef STARPORT_H
#define STARPORT_H

#include "my.h"


namespace iron
{



template<tid_t> class My;

class MyUnit;
FORWARD_DECLARE_MY(Terran_Siege_Tank_Tank_Mode)
FORWARD_DECLARE_MY(Terran_Vulture)
FORWARD_DECLARE_MY(Terran_SCV)


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Starport>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Starport> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

	vector<ConstructingAddonExpert *>	ConstructingAddonExperts() override {CI(this); return m_ConstructingAddonExperts; }

private:
	void					DefaultBehaviorOnFrame() override;

	static ExpertInConstructing<Terran_Starport>	m_ConstructingExpert;


	static vector<ConstructingAddonExpert *>	m_ConstructingAddonExperts;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Wraith>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Wraith> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					Cloak(check_t checkMode = check_t::check);
	int						MaxRepairers() const override						{ return 2; }
private:
	void					DefaultBehaviorOnFrame() override;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Valkyrie>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Valkyrie> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	int						MaxRepairers() const override						{ return 2; }

private:
	void					DefaultBehaviorOnFrame() override;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Science_Vessel>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Science_Vessel> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					DefensiveMatrix(MyBWAPIUnit * target, check_t checkMode = check_t::check);

	int						MaxRepairers() const override						{ return 2; }

private:
	void					DefaultBehaviorOnFrame() override;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Dropship>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Dropship> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					Update() override;

	void					Load(BWAPIUnit * u, check_t checkMode = check_t::check);
	void					Unload(BWAPIUnit * u, check_t checkMode = check_t::check);
	void					UnloadAll(check_t checkMode = check_t::check);

	int						MaxRepairers() const override			{ return 2; }

	bool					CanUnload() const;

	enum class damage_t {none, allow_attack, require_repair};
	damage_t				GetDamage() const;

	vector<MyUnit *>		LoadedUnits() const						{ return m_LoadedUnits; }
	MyUnit *				GetCargo(UnitType type) const;
	int						GetCargoCount(UnitType type) const;
	My<Terran_Siege_Tank_Tank_Mode> *		GetTank() const;
	My<Terran_Vulture> *					GetVulture() const;
	vector<My<Terran_SCV> *>				GetSCVs() const;
	bool					WorthBeingRepaired() const override;
	frame_t					LastUnload() const		{ return m_lastUnload; }
private:
	void					OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void					DefaultBehaviorOnFrame() override;
	vector<MyUnit *>		m_LoadedUnits;
	frame_t					m_lastUnload = 0;
};


My<Terran_Dropship>::damage_t dropshipDamage(const My<Terran_Dropship> * pDropship, const vector<MyUnit *> & ControlledUnits);


} // namespace iron


#endif

