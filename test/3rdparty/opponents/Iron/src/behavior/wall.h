//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EXECUTING_H
#define EXECUTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Executing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Executing : public Behavior<MyUnit>
{
public:
	static const vector<Executing *> &	Instances()					{ return m_Instances; }

	enum state_t {executing};

								Executing(MyUnit * pAgent);
								~Executing();

	const Executing *			IsExecuting() const override		{ return this; }
	Executing *					IsExecuting() override				{ return this; }

	string						Name() const override				{ return "executing"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

private:
	void						ChangeState(state_t st);

	state_t						m_state = executing;
	frame_t						m_inStateSince;
	frame_t						m_lastAttack = 0;
	MyUnit *					m_pLastAttackedTarget = nullptr;

	static vector<Executing *>	m_Instances;
};



} // namespace iron


#endif

