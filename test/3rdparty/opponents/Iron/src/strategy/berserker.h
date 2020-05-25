//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BERSERKER_H
#define BERSERKER_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Berserker
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Berserker : public Strat
{
public:
									Berserker();

	string							Name() const override { return "Berserker"; }
	string							StateDescription() const override;

	bool							Active() const;
	frame_t							ActiveSince() const	{ return m_activeSince; }

private:
	void							OnFrame_v() override;
	int								MineralsThreshold() const;
	int								GasThreshold() const;


	bool							m_active = false;
	frame_t							m_activeSince;
	frame_t							m_inactiveSince = 0;
	int								m_count = 0;
};


} // namespace iron


#endif

