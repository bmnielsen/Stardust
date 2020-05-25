//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SURROUND_H
#define SURROUND_H

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
FORWARD_DECLARE_MY(Terran_Vulture)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Surround
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Surround : public Strat
{
public:
									Surround();
									~Surround();

	string							Name() const override { return "Surround"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

private:
	bool							PreBehavior() const override		{ return true; }
	void							OnFrame_v() override;

	BWAPI::Color					GetColor() const					{ return Colors::Orange; }
	Text::Enum						GetTextColor() const				{ return Text::Orange; }
	bool							ShouldWatchEnemy(BWAPI::UnitType type) const;
	vector<My<Terran_Vulture> *>	FindCandidates() const;
	bool							m_active = false;
	frame_t							m_activeSince = 0;

	class EnemyGroup;
};


} // namespace iron


#endif

