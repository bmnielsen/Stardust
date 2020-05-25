//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef COLLECTING_H
#define COLLECTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)
FORWARD_DECLARE_MY(Terran_Siege_Tank_Tank_Mode)
FORWARD_DECLARE_MY(Terran_Dropship)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Collecting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Collecting : public Behavior<MyUnit>
{
public:
	static const vector<Collecting *> &	Instances()					{ return m_Instances; }

	enum state_t {reachingTarget, collecting};

	static bool					EnterCondition(My<Terran_Dropship> * pDropship);

								Collecting(My<Terran_Dropship> * pAgent);
								~Collecting();

	const Collecting *			IsCollecting() const override		{ return this; }
	Collecting *				IsCollecting() override				{ return this; }

	string						Name() const override				{ return "collecting"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	My<Terran_Dropship> *		ThisDropship() const;

private:
	MyUnit *					Find(UnitType type) const;
	void						ChangeState(state_t st);
	void						OnFrame_reachingTarget();
	void						OnFrame_collecting();

	state_t						m_state = reachingTarget;
	frame_t						m_inStateSince = 0;
	frame_t						m_lastMove = 0;
	int							m_initialLoadedUnits;

	static vector<Collecting *>	m_Instances;
};



} // namespace iron


#endif

