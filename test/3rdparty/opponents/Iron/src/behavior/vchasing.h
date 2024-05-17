//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VCHASING_H
#define VCHASING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../units/faceOff.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class HisUnit;

HisUnit * findVChasingAloneTarget(MyUnit * u);
HisUnit * findVChasingHelpSCVs(MyUnit * pMyUnit);

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VChasing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VChasing : public Behavior<MyBWAPIUnit>
{
public:
	static const vector<VChasing *> &	Instances()					{ return m_Instances; }


	enum state_t {chasing};

								VChasing(MyUnit * pAgent, BWAPIUnit * pTarget, bool dontFlee = false);
								~VChasing();

	const VChasing *			IsVChasing() const override			{ return this; }
	VChasing *					IsVChasing() override				{ return this; }

	string						Name() const override				{ return "chasing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Cyan; }
	Text::Enum					GetTextColor() const override		{ return Text::Cyan; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	BWAPIUnit *					Target() const						{CI(this); return m_pTarget; }
	const FaceOff &				GetFaceOff() const					{CI(this); return m_FaceOff; }

	frame_t						LastFrameTouchedHim() const			{CI(this); return m_lastFrameTouchedHim; }
	bool						TouchedHim() const					{CI(this); return m_touchedHim; }

	bool						NoMoreVChasers() const	{CI(this); return m_noMoreVChasers; }

private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_chasing();

	state_t						m_state = chasing;
	BWAPIUnit *					m_pTarget;
	FaceOff						m_FaceOff;
	frame_t						m_lastFrameTouchedHim;
	frame_t						m_lastFrameMovedToRandom = 0;
	frame_t						m_lastFrameAttack = 0;
	frame_t						m_firstFrameTouchedHim = 0;
	bool						m_touchedHim = false;

	frame_t						m_chasingSince;
	bool						m_noMoreVChasers = false;
	bool						m_dontFlee;

	static vector<VChasing *>	m_Instances;
};



} // namespace iron


#endif

