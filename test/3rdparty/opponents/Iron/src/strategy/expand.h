//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EXPAND_H
#define EXPAND_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class VBase;
class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)



VBase * findNatural(const VBase * pMain);

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Expand
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Expand : public Strat
{
public:
									Expand();
									~Expand();

	string							Name() const override { return "Expand"; }
	string							StateDescription() const override;

	bool							Active() const						{ return m_active; }
	bool							LiftCC() const;

private:
	bool							ConditionOnUnits() const;
	bool							ConditionOnActivity() const;
	bool							ConditionOnTech() const;
	bool							Condition() const;
	VBase *							FindNewBase() const;

	void							OnFrame_v() override;

	bool							m_active = false;
	frame_t							m_activeSince;
};


} // namespace iron


#endif

