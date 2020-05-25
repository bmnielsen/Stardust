//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef CHECKING_H
#define CHECKING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class HisBWAPIUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Checking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Checking : public Behavior<MyUnit>
{
public:
	static const vector<Checking *> &	Instances()					{ return m_Instances; }

	static MyUnit *				FindCandidate(HisBWAPIUnit * target);

	enum state_t {checking};

								Checking(MyUnit * pAgent, HisBWAPIUnit * target);
								~Checking();

	const Checking *			IsChecking() const override			{ return this; }
	Checking *					IsChecking() override				{ return this; }

	string						Name() const override				{ return "checking"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Orange; }
	Text::Enum					GetTextColor() const override		{ return Text::Orange; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	BWAPI::Unit					Target() const						{CI(this); return m_target; }

private:
	void						ChangeState(state_t st);

	state_t						m_state = checking;
	BWAPI::Unit					m_target;
	Position					m_where;
	frame_t						m_inStateSince;

	static vector<Checking *>	m_Instances;
};



} // namespace iron


#endif

