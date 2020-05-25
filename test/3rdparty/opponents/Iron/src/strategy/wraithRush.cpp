//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "wraithRush.h"
#include "baseDefense.h"
#include "../units/army.h"
#include "../behavior/VChasing.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class WraithRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


WraithRush::WraithRush()
{
}


string WraithRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


bool WraithRush::Detection() const
{
	const HisBuilding * pHisStarport = nullptr;
	for (const auto & b : him().Buildings())
		if (b->Is(Terran_Starport))
			if (pHisStarport) return true;	// he's got 2 Starports;
			else pHisStarport = b.get();

	if (pHisStarport)
	{
		const MyBuilding * pMyStarport = me().Buildings(Terran_Starport).empty() ? nullptr : me().Buildings(Terran_Starport).front().get();

		if (!pMyStarport ||
			pHisStarport->Completed() && !pMyStarport->Completed() ||
			!pHisStarport->InFog() && (pHisStarport->RemainingBuildTime() < pMyStarport->RemainingBuildTime()))
			return true;
	}

	if ((int)him().AllUnits(Terran_Wraith).size() > me().CompletedUnits(Terran_Wraith))
		return true;

//	if (me().Units(Terran_Vulture).size() >= 5) return true;

	return false;
}


bool WraithRush::ConditionToMakeMarines() const
{
	if (m_detected)
		if (me().CompletedBuildings(Terran_Missile_Turret) < 2 * (int)me().Bases().size())
			if (me().Units(Terran_Marine).size() < him().AllUnits(Terran_Wraith).size() + 2)
				if (any_of(him().Units(Terran_Wraith).begin(), him().Units(Terran_Wraith).end(), [](const HisUnit * u)
					{ return findMyClosestBase(u->Pos(), 4); }))
					return true;

	return false;
}

void WraithRush::OnFrame_v()
{
	if (him().IsProtoss() || him().IsZerg()) return Discard();

	if (me().Units(Terran_Goliath).size() >= 4) return Discard();

	if (m_detected)
	{
		for (const auto & b : me().Buildings(Terran_Starport))
			if (!b->Completed())
				if (b->CanAcceptCommand())
					DO_ONCE
						return b->CancelConstruction();

		static frame_t lastCancel = 0;
		if (ai()->Frame() - lastCancel > 3*bw->getRemainingLatencyFrames())
		for (const auto & b : me().Buildings(Terran_Starport))
			if (b->Unit()->isTraining())
				if (b->CanAcceptCommand())
				{
				///	bw << "CancelTrain Wraith" << endl;
				///	ai()->SetDelay(5000);
					lastCancel = ai()->Frame();
					return b->CancelTrain();
				}

		for (HisUnit * wraith : him().Units(Terran_Wraith))
			if (findMyClosestBase(wraith->Pos(), 4))
				for (const auto & u : me().Units(Terran_Marine))
					if (!u->GetBehavior()->IsVChasing())
						u->ChangeBehavior<VChasing>(u.get(), wraith, bool("dontFlee"));
	}
	else
	{
		if (me().CreationCount(Terran_Wraith) >= 3) return Discard();

		if (Detection())
		{
		///	ai()->SetDelay(100);
			m_detected = true;
			return;
		}
	}
}


} // namespace iron



