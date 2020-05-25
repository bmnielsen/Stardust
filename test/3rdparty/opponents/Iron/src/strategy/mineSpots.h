//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef MINE_SPOTS_H
#define MINE_SPOTS_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MineSpots
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class MineSpots : public Strat
{
public:
									MineSpots();
									~MineSpots();

	string							Name() const override { return "MineSpots"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

private:
	void							OnFrame_v() override;
	Position						FindSpot() const;
	int								FilledSpots() const	{ return m_filledSpots; }

	bool							m_active = false;
	frame_t							m_lastMinePlacement = 0;
	mutable int						m_filledSpots;
};


} // namespace iron


#endif

