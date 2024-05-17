//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef RETRAITING_H
#define RETRAITING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class FaceOff;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Retraiting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Retraiting : public Behavior<MyUnit>
{
public:
	static const vector<Retraiting *> &	Instances()					{ return m_Instances; }

	static bool					Allowed();
	static int					Condition(const MyUnit * u, const vector<const FaceOff *> & Threats);

	enum state_t {retraiting};

								Retraiting(MyUnit * pAgent, Position target, int numberNeeded);
								Retraiting(MyUnit * pAgent, Retraiting * pExistingGroup);
								~Retraiting();

	const Retraiting *			IsRetraiting() const override		{ return this; }
	Retraiting *				IsRetraiting() override				{ return this; }

	string						Name() const override				{ return "Retraiting"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Blue; }
	Text::Enum					GetTextColor() const override		{ return Text::Blue; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	Position					Target() const						{CI(this); return m_target; }
	const Area *				TargetArea() const					{CI(this); return m_pTargetArea; }
	frame_t						InStateSince() const				{CI(this); return m_inStateSince;; }

private:
	void						ChangeState(state_t st);

	state_t						m_state = retraiting;
	Position					m_target;
	const Area *				m_pTargetArea;
	frame_t						m_inStateSince;
	int							m_numberNeeded;
	bool						m_growed = false;
	bool						m_releaseGroupWhenLeaving = true;
	shared_ptr<vector<Retraiting *>>	m_pGroup;

	static vector<Retraiting *>	m_Instances;
	static bool					m_allowed;
};



} // namespace iron


#endif

