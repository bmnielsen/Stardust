//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef WALKING_H
#define WALKING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VBase;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Walking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Walking : public Behavior<MyBWAPIUnit>
{
public:
	enum state_t {intermediate, final, failed, succeeded};

								Walking(MyBWAPIUnit * pAgent, Position target, const string & callerFileAndLine);
								~Walking();

	const Walking *				IsWalking() const override			{ return this; }
	Walking *					IsWalking() override				{ return this; }

	string						Name() const override				{ return "walking"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	Position					Target() const						{CI(this); return m_target; }
	Position					CurrentWayPoint() const;

private:
	void						ChangeState(state_t st)				{CI(this); assert_throw(m_state != st); m_state = st; OnStateChanged(); }
	void						OnFrame_intermediate();
	void						OnFrame_final();
	bool						ShouldPatrol() const;

	const Position				m_target;
	const CPPath &				m_Path;
	int							m_length;
	int							m_index = 0;
	int							m_targetRadius = 1;
	int							m_tries = 0;
	state_t						m_state = intermediate;
};



} // namespace iron


#endif

