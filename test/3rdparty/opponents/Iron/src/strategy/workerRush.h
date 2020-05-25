//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef WORKER_RUSH_H
#define WORKER_RUSH_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"



namespace iron
{

class MyUnit;
class HisKnownUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class WorkerRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class WorkerRush : public Strat
{
public:
									WorkerRush();
									~WorkerRush();

	string							Name() const override { return "WorkerRush"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

	int								MaxMarines() const;

	void							WorkerDefense();

private:
	void							OnFrame_v() override;
	bool							Detection() const;
	void							UpdateHisRushers() const;

	bool							m_detected = false;
	mutable vector<const HisKnownUnit *>	m_HisRushers;
};


} // namespace iron


#endif

