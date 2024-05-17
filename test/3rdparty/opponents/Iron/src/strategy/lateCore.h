//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef LATE_CORE_H
#define LATE_CORE_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class LateCore
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class LateCore : public Strat
{
public:
									LateCore();
									~LateCore();

	string							Name() const override { return "LateCore"; }
	string							StateDescription() const override;

	bool							Active() const { return m_active; }

private:
	void							OnFrame_v() override;
	frame_t							m_timeToReachHisBase = 0;
	frame_t							m_watchLateGatewaySince = 0;
	bool							m_active = false;
};


} // namespace iron


#endif

