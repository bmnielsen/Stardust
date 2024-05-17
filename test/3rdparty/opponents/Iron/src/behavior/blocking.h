//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BLOCKING_H
#define BLOCKING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VChokePoint;
class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Blocking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Blocking : public Behavior<MyUnit>
{
public:
	static const vector<Blocking *> &	Instances()					{ return m_Instances; }

								Blocking(MyUnit * pAgent, VChokePoint * pWhere);
								~Blocking();

	enum state_t {reachingCP, blocking, dragOut};

	const Blocking *			IsBlocking() const override			{ return this; }
	Blocking *					IsBlocking() override				{ return this; }

	string						Name() const override				{ return "blocking"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{CI(this); if (State() == dragOut) return Colors::Green; return m_pTarget ? Colors::Cyan : m_pRepaired ? Colors::Yellow : Colors::Blue; }
	Text::Enum					GetTextColor() const override		{CI(this); if (State() == dragOut) return Text::Green; return m_pTarget ? Text::Cyan : m_pRepaired ? Text::Yellow : Text::Blue; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int) const override;
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	VChokePoint *				Where() const						{CI(this); return m_pWhere; }
	HisUnit *					Target() const						{CI(this); return m_pTarget; }

private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_reachingCP();
	void						OnFrame_blocking();
	void						OnFrame_dragOut();

	state_t						m_state = reachingCP;
	frame_t						m_inStateSince;
	VChokePoint *				m_pWhere;
	HisUnit *					m_pTarget = nullptr;
	Blocking *					m_pRepaired = nullptr;

	static vector<Blocking *>	m_Instances;
};



} // namespace iron


#endif

