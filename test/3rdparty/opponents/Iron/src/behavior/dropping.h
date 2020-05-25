//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DROPPING_H
#define DROPPING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Dropping : public Behavior<MyUnit>
{
public:
	static const vector<Dropping *> &	Instances()					{ return m_Instances; }

	enum state_t {executing};

								Dropping(MyUnit * pAgent);
								~Dropping();

	const Dropping *			IsDropping() const override		{ return this; }
	Dropping *					IsDropping() override				{ return this; }

	string						Name() const override				{ return "dropping"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	MyUnit *					RepairedUnit()						{ return m_pRepairedUnit; }

	state_t						State() const						{CI(this); return m_state; }

private:
	MyUnit *					FindDamagedUnit() const;
	int							MinLifeUntilLoad() const;
	void						ChangeState(state_t st);

	state_t						m_state = executing;
	frame_t						m_inStateSince;
	frame_t						m_lastAttack = 0;
	MyUnit *					m_pLastAttackedTarget = nullptr;
	MyUnit *					m_pRepairedUnit = nullptr;

	static vector<Dropping *>	m_Instances;
};



} // namespace iron


#endif

