//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef HARASSING_H
#define HARASSING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VBase;
class HisUnit;
class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Harassing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Harassing : public Behavior<My<Terran_SCV>>
{
public:
	static const vector<Harassing *> &	Instances()					{ return m_Instances; }

	enum state_t {reachingHisBase, retreating, attackNearest};

								Harassing(My<Terran_SCV> * pAgent);
								~Harassing();

	const Harassing *			IsHarassing() const override		{ return this; }
	Harassing *					IsHarassing() override				{ return this; }

	string						Name() const override				{ return "harassing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Teal; }
	Text::Enum					GetTextColor() const override		{ return Text::Teal; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * pTarget) const override;

	state_t						State() const						{CI(this); return m_state; }
	VBase *						HisBase() const						{CI(this); return m_pHisBase; }

private:
	const MyUnit *				FindFrontSoldier() const;
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_reachingHisBase();
	void						OnFrame_retreating();
	void						OnFrame_attackNearest();

	void						OnFrame_noEnemyWorker();


	state_t						m_state = reachingHisBase;
	VBase *						m_pHisBase;
	HisUnit *					m_pCurrentTarget = nullptr;
	map<HisUnit *, int>			m_prevDistances;
	int							m_lifeBeforeAttack;
	frame_t						m_inStateSince;
	frame_t						m_lastAttack = 0;
	frame_t						m_timesCouldChaseRatherThanAttackNearest;
	int							m_setbackDist;

	static vector<Harassing *>	m_Instances;
};



} // namespace iron


#endif

