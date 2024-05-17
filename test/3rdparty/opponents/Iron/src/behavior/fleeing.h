//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FLEEING_H
#define FLEEING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class HisUnit;
class MyUnit;
class FaceOff;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fleeing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Fleeing : public Behavior<MyUnit>
{
public:
	static const vector<Fleeing *> &	Instances()					{ return m_Instances; }

	enum state_t {dragOut, insideArea, anywhere};

								Fleeing(MyUnit * pAgent, frame_t noAlertFrames = 0);
								~Fleeing();

	const Fleeing *				IsFleeing() const override			{ return this; }
	Fleeing *					IsFleeing() override				{ return this; }

	string						Name() const override				{ return "fleeing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Green; }
	Text::Enum					GetTextColor() const override		{ return Text::Green; }

	state_t						State() const						{CI(this); return m_state; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int) const override;
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	double						AvgPursuers() const					{CI(this); return m_avgPursuers; }
	bool						CanReadAvgPursuers() const			{CI(this); return m_canReadAvgPursuers; }

	frame_t						InStateSince() const				{CI(this); return m_inStateSince; }

	void						OnMineralDestroyed(Mineral * m);

private:
	void						ChangeState(state_t st);
	void						OnFrame_dragOut();
	void						OnFrame_anywhere();

	Vect						CalcultateThreatVector(const vector<const FaceOff *> & Threats) const;
	Vect						CalcultateDivergingVector() const;
	Vect						CalcultateIncreasingAltitudeVector() const;
	Vect						CalcultateAntiAirVector() const;
	void						AdjustTarget(Position & target, const int walkRadius) const;
	bool						AttackedAndSurrounded() const;
	void						Alert();
	
	state_t						m_state;
	Position					m_lastTarget;
	Position					m_lastPos;
	frame_t						m_inStateSince;
	frame_t						m_remainingFramesInFreeState;
	int							m_cumulativePursuers = 0;
	int							m_numberOf_pursuersCount = 0;
	bool						m_canReadAvgPursuers = false;
	double						m_avgPursuers = 0.0;
	bool						m_onlyWorkerPursuers;
	frame_t						m_noAlertUntil;
	Mineral *					m_pDragOutTarget = nullptr;
	enum { size_minDistToWorkerPursuerRange = 4};
	int							m_minDistToWorkerPursuerRange[size_minDistToWorkerPursuerRange];
	bool						m_flyingPursuers;
	
	static vector<Fleeing *>	m_Instances;
};


vector<const FaceOff *> findPursuers(const MyUnit * pAgent);
vector<const FaceOff *> findThreats(const MyUnit * pAgent, int maxDistToHisRange, int * pDistanceToHisFire = nullptr, bool skipAirUnits = false);
vector<const FaceOff *> findThreatsButWorkers(const MyUnit * pAgent, int maxDistToHisRange, int * pDistanceToHisFire = nullptr);

} // namespace iron


#endif

