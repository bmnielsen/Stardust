//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BASE_DEFENSE_H
#define BASE_DEFENSE_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class HisKnownUnit;
class VBase;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BaseDefense
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class BaseDefense : public Strat
{
public:
									BaseDefense();
									~BaseDefense();

	string							Name() const override { return "BaseDefense"; }
	string							StateDescription() const override;

	bool							Active() const				{ return m_active; }
	VBase *							AttackedBase() const		{ return m_pAttackedBase; }

private:
	int								EvalVulturesNeededAgainst_Terran(const vector<const HisKnownUnit *> & Threats) const;
	int								EvalVulturesNeededAgainst_Protoss(const vector<const HisKnownUnit *> & Threats) const;
	int								EvalVulturesNeededAgainst_Zerg(const vector<const HisKnownUnit *> & Threats) const;
	int								EvalVulturesNeededAgainst(const vector<const HisKnownUnit *> & Threats) const;
	void							OnFrame_v() override;

	bool							m_active = false;
	frame_t							m_activeSince = 0;
	VBase *							m_pAttackedBase = nullptr;
};


VBase * findMyClosestBase(Position pos, int maxTileDistanceToArea = 10);

} // namespace iron


#endif

