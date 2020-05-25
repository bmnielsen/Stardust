//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef KILLING_MINE_H
#define KILLING_MINE_H

#include <BWAPI.h>
#include "behavior.h"
#include "../units/faceOff.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class HisBWAPIUnit;
class HisUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class KillingMine
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class KillingMine : public Behavior<MyUnit>
{
public:
	static const vector<KillingMine *> &	Instances()					{ return m_Instances; }

	enum state_t {killingMine};

								KillingMine(MyUnit * pAgent, HisBWAPIUnit * pTarget);
								~KillingMine();

	const KillingMine *			IsKillingMine() const override		{ return this; }
	KillingMine *				IsKillingMine() override			{ return this; }

	string						Name() const override				{ return "killingMine"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Orange; }
	Text::Enum					GetTextColor() const override		{ return Text::Orange; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	HisBWAPIUnit *				Target() const						{CI(this); return m_pTarget; }


private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	HisBWAPIUnit *				NearestTarget() const;
	void						ChangeState(state_t st);

	state_t						m_state = killingMine;
	frame_t						m_inStateSince;
	HisBWAPIUnit *				m_pTarget;
	FaceOff						m_FaceOff;
	frame_t						m_lastFrameAttack = 0;

	static vector<KillingMine *>	m_Instances;
};



} // namespace iron


#endif

