//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef ARMY_H
#define ARMY_H

#include <BWAPI.h>
#include "../defs.h"


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Army
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Army
{
public:
	enum {sizePrevValue = 32};

									Army();

	void							Update();

	bool							GroundLead() const				{ return m_groundLead; }
	const string &					GroundLeadStatus_marines() const	{ return m_groundLeadStatus_marines; }
	const string &					GroundLeadStatus_vultures() const	{ return m_groundLeadStatus_vultures; }
	const string &					GroundLeadStatus_mines() const		{ return m_groundLeadStatus_mines; }
	bool							MarinesCond() const			{ return m_marinesCond; }
	bool							VulturesCond() const			{ return m_vulturesCond; }
	bool							MinesCond() const				{ return m_minesCond; }
	int								MyGroundUnitsAhead() const		{ return m_myGroundUnitsAhead; }
	int								HisGroundUnitsAhead() const		{ return m_hisGroundUnitsAhead; }

	// These ratios are tenths of all the vultures + tanks + goliaths:
	int								TankRatioWanted() const			{ return m_tankRatioWanted; }
	int								GoliathRatioWanted() const		{ return m_goliathRatioWanted; }
	bool							FavorTanksOverGoliaths() const	{ return m_favorTanksOverGoliaths; }

	// tenth
	int								AirAttackOpportunity() const	{ return m_hisSoldiersFearingAir * 10 / max(1, max(m_hisSoldiersFearingAir, m_hisSoldiers)); }

	int								HisInvisibleUnits() const		{ return m_hisInvisibleUnits; }


	// Updated every 50 frames.
	int								Value() const					{ return m_prevValue[0]; }
	int								PrevValue(int i) const			{ assert_throw((0 <= i) && (i < sizePrevValue)); return m_prevValue[i]; }
	bool							GoodValueInTime() const			{ return m_goodValueInTime; }

	bool							KeepGoliathsAtHome() const;
	bool							KeepTanksAtHome() const;

private:
	void							UpdateStats();
	void							UpdateValue();
	void							UpdateHisArmyStats();
	void							UpdateGroundLead();
	void							UpdateRatios();
	void							UpdateRatios_vsTerran();
	void							UpdateRatios_vsProtoss();
	void							UpdateRatios_vsZerg();

	int								m_hisSoldiers;
	int								m_hisSoldiersFearingAir;
	int								m_hisInvisibleUnits;

	int								m_prevValue[sizePrevValue];
	bool							m_goodValueInTime;

	int								m_myGroundUnitsAhead = -1;
	int								m_hisGroundUnitsAhead = -1;

	bool							m_marinesCond;
	bool							m_vulturesCond;
	bool							m_minesCond;
	bool							m_groundLead;
	string							m_groundLeadStatus_marines;
	string							m_groundLeadStatus_vultures;
	string							m_groundLeadStatus_mines;

	int								m_tankRatioWanted = 2;
	int								m_goliathRatioWanted = 0;

	bool							m_favorTanksOverGoliaths;
};



} // namespace iron


#endif

