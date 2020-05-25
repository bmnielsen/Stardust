//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DROP_THREAT_H
#define DROP_THREAT_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DropThreat
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class DropThreat : public Strat
{
public:
									DropThreat();

	string							Name() const override { return "DropThreat"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }


private:
	void							OnFrame_v() override;

	bool							m_detected = false;
	frame_t							m_detetedSince;
};


} // namespace iron


#endif

