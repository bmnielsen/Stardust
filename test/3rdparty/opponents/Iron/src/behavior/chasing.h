//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef CHASING_H
#define CHASING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../units/faceOff.h"
#include "../utils.h"


namespace iron
{

class HisUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Chasing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Chasing : public Behavior<MyBWAPIUnit>
{
public:
	static const vector<Chasing *> &	Instances()					{ return m_Instances; }

	static Chasing *			GetChaser(HisUnit * pTarget, int maxDist = 8*32);

	enum state_t {chasing};

								Chasing(MyUnit * pAgent, BWAPIUnit * pTarget, bool insist = false, frame_t maxChasingTime = 0, bool workerDefense = false);
								~Chasing();

	const Chasing *				IsChasing() const override			{ return this; }
	Chasing *					IsChasing() override				{ return this; }

	string						Name() const override				{CI(this); return "chasing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Cyan; }
	Text::Enum					GetTextColor() const override		{ return Text::Cyan; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	BWAPIUnit *					Target() const						{CI(this); return m_pTarget; }
	const FaceOff &				GetFaceOff() const					{CI(this); return m_FaceOff; }

	frame_t						LastFrameTouchedHim() const			{CI(this); return m_lastFrameTouchedHim; }
	bool						TouchedHim() const					{CI(this); return m_touchedHim; }

	bool						WorkerDefense() const				{CI(this); return m_workerDefense; }

	bool						NoMoreChasers() const	{CI(this); return m_noMoreChasers; }

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
	frame_t						m_maxChasingTime;
	bool						m_touchedHim = false;

	bool						m_insist;
	frame_t						m_chasingSince;
	bool						m_noMoreChasers = false;
	bool						m_workerDefense;

	static vector<Chasing *>	m_Instances;
};



} // namespace iron


#endif

