//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "workerRush.h"
#include "../strategy/strategy.h"
#include "../behavior/scouting.h"
#include "../behavior/constructing.h"
#include "../behavior/chasing.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class WorkerRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


WorkerRush::WorkerRush()
{
}


WorkerRush::~WorkerRush()
{
	ai()->GetStrategy()->SetMinScoutingSCVs(1);
}


string WorkerRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


int WorkerRush::MaxMarines() const
{
	return m_HisRushers.size() + 1;
}


void WorkerRush::UpdateHisRushers() const
{
	vector<const Area *> MyTerritory = me().EnlargedAreas();
	for (const Area * area : me().EnlargedAreas())
		for (const Area * neighbour : area->AccessibleNeighbours())
			push_back_if_not_found(MyTerritory, neighbour);


	m_HisRushers.clear();
	for (const auto & u : him().AllUnits())
		if (u.second.Type().isWorker())
			if (ai()->Frame() - u.second.LastTimeVisible() < 300)
				if (const Area * area = ai()->GetMap().GetNearestArea(WalkPosition(u.second.LastPosition())))
					if (contains(MyTerritory, area))
						m_HisRushers.push_back(&u.second);
}


void WorkerRush::WorkerDefense()
{
	vector<HisUnit *> HisVisibleRushers;
	for (const HisKnownUnit * u : m_HisRushers)
		if (u->GetHisUnit())
			HisVisibleRushers.push_back(u->GetHisUnit());

	if (HisVisibleRushers.empty()) return;

	vector<Chasing *> MyChasers;
	for (Chasing * chaser : Chasing::Instances())
		if (!chaser->WorkerDefense())
			MyChasers.push_back(chaser);

	if (MyChasers.size() == HisVisibleRushers.size()) return;
	if (MyChasers.size() > HisVisibleRushers.size())
	{
		MyUnit * pSCV = MyChasers.front()->Agent()->IsMyUnit();
		pSCV->ChangeBehavior<DefaultBehavior>(pSCV);
		return;
	}

	for (const auto & u : me().Units(Terran_SCV))
		if (u->Completed() && !u->Loaded())
			if (u->GetStronghold())
				if (!u->GetBehavior()->IsChasing())
				if (!u->GetBehavior()->IsRepairing())
				if (!u->GetBehavior()->IsConstructing())
					if (u->Life() == 60)
					{
						u->ChangeBehavior<Chasing>(u.get(), HisVisibleRushers.front(), bool("insist"), 300);
						return;
					}
}


bool WorkerRush::Detection() const
{
	
	if (m_HisRushers.size() >= 3) return true;

	return false;
}


void WorkerRush::OnFrame_v()
{
	UpdateHisRushers();

	if (m_detected)
	{
		if (me().Units(Terran_Vulture).size() >= 3) return Discard();

		if (me().Units(Terran_SCV).size() >= 15)
			if (me().Units(Terran_Marine).size() > 2 + m_HisRushers.size()/2 ||
				me().Units(Terran_Marine).size() > 8 && me().MineralsAvailable() > 300)
			{
				// Quickly counterattack the enemy base with a bunch of Scouting SCVs.
				DO_ONCE ai()->GetStrategy()->SetMinScoutingSCVs(4);

				// Since there may be Mining SCVs lacking,
				// we don't want the new Scouting SCVs to go back to home.
				for (const auto & u : me().Units(Terran_SCV))
					if (auto * pSCV = u->IsMy<Terran_SCV>())
						if (!pSCV->GetStronghold())
							pSCV->SetSoldierForever();

				if (me().SCVsoldiers() == 4)
					return Discard();

				if (me().MineralsAvailable() > 500)
					return Discard();
			}

		for (const auto & b : me().Buildings(Terran_Refinery))
			if (!b->Completed())
				if (b->CanAcceptCommand())
					DO_ONCE
						return b->CancelConstruction();

		WorkerDefense();

		DO_ONCE
			for (Constructing * pBuilder : Constructing::Instances())
				if (pBuilder->Type() == Terran_Refinery)
					return pBuilder->Agent()->ChangeBehavior<DefaultBehavior>(pBuilder->Agent());
	}
	else
	{
		if (Detection())
		{
		///	ai()->SetDelay(100);
			m_detected = true;
			return;
		}

		if (me().Units(Terran_Vulture).size() >= 2) return Discard();
		if (me().Units(Terran_SCV).size() >= 25) return Discard();
	}
}


} // namespace iron



