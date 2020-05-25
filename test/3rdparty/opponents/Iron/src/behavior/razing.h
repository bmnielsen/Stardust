//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef RAZING_H
#define RAZING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class HisBuilding;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Razing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Razing : public Behavior<MyUnit>
{
public:
	static const vector<Razing *> &	Instances()						{ return m_Instances; }
	static bool					Condition(MyUnit * u)				{ return FindBuilding(u) != nullptr; }

								Razing(MyUnit * pAgent, const Area * pWhere);
								~Razing();

	enum state_t {razing};

	const Razing *				IsRazing() const override			{ return this; }
	Razing *					IsRazing() override					{ return this; }

	string						Name() const override				{ return "razing"; }
	string						StateName() const override			{ return "-"; }


	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return true; }
	bool						CanChase(const HisUnit * ) const override			{ return true; }

	state_t						State() const						{CI(this); return m_state; }
	const Area *				Where() const						{CI(this); return m_pWhere; }

private:
	void						ChangeState(state_t st);
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;

	HisBuilding *				FindBuilding() const				{ return FindBuilding(Agent()); }
	static HisBuilding *		FindBuilding(MyUnit * u);

	state_t						m_state = razing;
	frame_t						m_inStateSince;
	const Area *				m_pWhere;
	HisBuilding *				m_pTarget = nullptr;
	HisBuilding *				m_pLastAttackedTarget = nullptr;

	static vector<Razing *>	m_Instances;
};



} // namespace iron


#endif

