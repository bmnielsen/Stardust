//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef UNBLOCK_TRAFFIC_H
#define UNBLOCK_TRAFFIC_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
struct GridMapCell;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class UnblockTraffic
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class UnblockTraffic : public Strat
{
public:
									UnblockTraffic();
									~UnblockTraffic();

	string							Name() const override { return "UnblockTraffic"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

private:
	void							OnFrame_v() override;
	void							OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit) override;

	vector<const GridMapCell *>		Update();
	bool							Update(const GridMapCell & Cell, const vector<MyUnit *> & Candidates);
	vector<MyUnit *>				FindCandidates(const GridMapCell & Cell, int minCount) const;
	pair<const GridMapCell *, const ChokePoint *> SelectCellNearCP(vector<const GridMapCell *> CandidateCells) const;
	void							FreeMovedUnits();

	bool							m_active = false;
	frame_t							m_activeSince;
	vector<MyUnit *>				m_MovedUnits;
	Position						m_target;
	const ChokePoint *				m_pCP;

	map<const GridMapCell *, vector<MyUnit *>>	m_PreviousCandidates;
};


} // namespace iron


#endif

