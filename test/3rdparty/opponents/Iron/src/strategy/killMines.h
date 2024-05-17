//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef KILL_MINES_H
#define KILL_MINES_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class HisKnownUnit;
class HisUnit;
class VBase;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class KillMines
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class KillMines : public Strat
{
public:
									KillMines();
									~KillMines();

	string							Name() const override { return "KillMines"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

private:
	bool							PreBehavior() const override		{ return true; }
	void							OnFrame_v() override;
	void							Process(HisUnit * pMine, bool dangerous);

	bool							m_active = false;
	frame_t							m_activeSince = 0;
	vector<HisUnit *>				m_OldDangerousMines;
};

} // namespace iron


#endif

