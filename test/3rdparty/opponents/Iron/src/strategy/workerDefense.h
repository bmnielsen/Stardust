//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef WORKER_DEFENSE_H
#define WORKER_DEFENSE_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class BWAPIUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class WorkerDefense
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class WorkerDefense : public Strat
{
public:
									WorkerDefense();
									~WorkerDefense();

	string							Name() const override { return "WorkerDefense"; }
	string							StateDescription() const override;

	bool							Active() const;

private:
	bool							PreBehavior() const override		{ return true; }
	void							OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit) override;
	void							OnFrame_v() override;
};


} // namespace iron


#endif

