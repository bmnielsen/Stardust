//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EARLY_RUN_BY_H
#define EARLY_RUN_BY_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class EarlyRunBy
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class EarlyRunBy : public Strat
{
public:
									EarlyRunBy();
									~EarlyRunBy();

	string							Name() const override { return "EarlyRunBy"; }
	string							StateDescription() const override;

	bool							Active() const				{ return m_started; }
	bool							Started() const				{ return m_started; }
	bool							InSquad(MyUnit * u) const	{ return m_started && contains(m_Squad, u); }

private:
	void							OnFrame_v() override;
	void							OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit) override;

	bool							m_started = false;
	bool							m_mayGoBack;
	frame_t							m_startedSince;
	vector<MyUnit *>				m_Squad;
	int								m_tries = 0;
	frame_t							m_waitingFramesIfFewSoldiers = -1;
};


} // namespace iron


#endif

