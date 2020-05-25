//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef MARINES_RUSH_H
#define MARINES_RUSH_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MarineRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class MarineRush : public Strat
{
public:
									MarineRush();

	string							Name() const override { return "MarineRush"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

	int								MaxMarines() const;

private:
	void							OnFrame_v() override;

	bool							m_snipersAvailable = false;
	bool							m_detected = false;
	frame_t							m_detetedSince;
	frame_t							m_noThreatSince;
};


} // namespace iron


#endif

