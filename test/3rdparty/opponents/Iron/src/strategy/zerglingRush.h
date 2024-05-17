//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef ZERGLING_RUSH_H
#define ZERGLING_RUSH_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ZerglingRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class ZerglingRush : public Strat
{
public:
									ZerglingRush();
									~ZerglingRush();

	string							Name() const override { return "ZerglingRush"; }
	string							StateDescription() const override;

	bool							Detected() const		{ return m_detected; }

	int								MaxMarines() const		{ return m_maxMarines; }
	bool							TechRestartingCondition() const;

	void							SetNoLocationForBunkerForDefenseCP()	{ m_noLocationForBunkerForDefenseCP = true; }
	bool							NoLocationForBunkerForDefenseCP() const	{ return m_noLocationForBunkerForDefenseCP; }
	bool							SnipersAvailable() const				{ return m_snipersAvailable; }
	bool							FastPool() const ;
	int								TimeZerglingHere() const				{ return m_timeZerglingHere; }

private:
	void							OnFrame_v() override;
	void							ChokePointDefense();
	void							WorkerDefense();

	bool							m_detected = false;
	frame_t							m_detetedSince;
	bool							m_noLocationForBunkerForDefenseCP = false;
	bool							m_snipersAvailable = false;
	mutable bool					m_techRestarting = false;
	//TilePosition					m_BunkerLocation = TilePositions::None;
	int								m_zerglings = 0;
	int								m_hypotheticZerglings = 0;
	int								m_maxMarines = 4;
	int								m_blockersWanted = 0;
	frame_t							m_timeZerglingHere = 0;
	frame_t							m_zerglingCompletedTime = 0;
};


void marineHandlingOnZerglingOrHydraPressure();

} // namespace iron


#endif

