//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef TERRAN_FAST_EXPAND_H
#define TERRAN_FAST_EXPAND_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TerranFastExpand
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class TerranFastExpand : public Strat
{
public:
									TerranFastExpand();

	string							Name() const override { return "TerranFastExpand"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

private:
	void							OnFrame_v() override;
	bool							Detection() const;

	bool							m_detected = false;
};


} // namespace iron


#endif

