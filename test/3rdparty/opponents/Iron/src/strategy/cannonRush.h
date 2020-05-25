//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef CANNON_RUSH_H
#define CANNON_RUSH_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class HisBuilding;

FORWARD_DECLARE_MY(Terran_SCV)
//class HisBuilding;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class CannonRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class CannonRush : public Strat
{
public:
									CannonRush();
									~CannonRush();

	string							Name() const override { return "CannonRush"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

	vector<Position>				HotPositions() const;
	int								DistanceToHotPositions(Position from) const;

private:
	void							OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit) override;
	void							OnFrame_v() override;
	int								WantedDefenders(const HisBuilding * pHisBuilding, int countPylons, int countCannons) const;

	bool							m_detected = false;
	frame_t							m_detetedSince;
	frame_t							m_noThreatSince;
	frame_t							m_waitingFramesBeforeCancelFactory;
	bool							m_noMoreDefenders = false;

	enum pylonStatus {to_visit, visited};
	map<BWAPI::Unit, vector<My<Terran_SCV> *>>	m_map_HisBuilding_MyDefenders;
	vector<BWAPI::Unit>				m_HisBuildingsProtected;
};


} // namespace iron


#endif

