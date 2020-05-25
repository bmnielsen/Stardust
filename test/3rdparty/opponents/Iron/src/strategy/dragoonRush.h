//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DRAGOON_RUSH_H
#define DRAGOON_RUSH_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DragoonRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class DragoonRush : public Strat
{
public:
									DragoonRush();

	string							Name() const override { return "DragoonRush"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

	bool							ConditionToStartSecondShop() const;

private:
	void							OnFrame_v() override;

	bool							m_detected = false;
	frame_t							m_detetedSince;
};


} // namespace iron


#endif

