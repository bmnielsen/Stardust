//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FIGHT_H
#define FIGHT_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class HisKnownUnit;
class MyUnit;
class HisUnit;
class VBase;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Fight : public Strat
{
public:
									Fight();
									~Fight();

	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

private:
	bool							PreBehavior() const override		{ return true; }
	void							OnFrame_v() override;

	virtual zone_t					Zone() const = 0;
	virtual BWAPI::Color			GetColor() const = 0;
	virtual Text::Enum				GetTextColor() const = 0;
	virtual bool					ShouldWatchEnemy(BWAPI::UnitType type) const = 0;
	virtual vector<MyUnit *>		FindCandidates() const = 0;
	bool							m_active = false;
	frame_t							m_activeSince = 0;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class GroundFight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class GroundFight : public Fight
{
public:
									GroundFight();

	string							Name() const override { return "GroundFight"; }

private:
	zone_t							Zone() const override				{ return zone_t::ground; }
	BWAPI::Color					GetColor() const override			{ return Colors::White; }
	Text::Enum						GetTextColor() const override		{ return Text::White; }
	bool							ShouldWatchEnemy(BWAPI::UnitType type) const override;
	vector<MyUnit *>				FindCandidates() const override;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class AirFight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class AirFight : public Fight
{
public:
									AirFight();

	string							Name() const override { return "AirFight"; }

private:
	zone_t							Zone() const override				{ return zone_t::air; }
	BWAPI::Color					GetColor() const override			{ return Colors::Cyan; }
	Text::Enum						GetTextColor() const override		{ return Text::Cyan; }
	bool							ShouldWatchEnemy(BWAPI::UnitType type) const;
	vector<MyUnit *>				FindCandidates() const override;

};

} // namespace iron


#endif

