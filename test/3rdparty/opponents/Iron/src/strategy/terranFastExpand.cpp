//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "terranFastExpand.h"
#include "strategy.h"
#include "wraithRush.h"
#include "../units/army.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TerranFastExpand
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


TerranFastExpand::TerranFastExpand()
{
}


string TerranFastExpand::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


bool TerranFastExpand::Detection() const
{
/*
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
*/
	if (him().AllUnits(Terran_Siege_Tank_Tank_Mode).size() > him().AllUnits(Terran_Vulture).size() ||
		((int)him().AllUnits(Terran_Siege_Tank_Tank_Mode).size()*3 > me().CompletedUnits(Terran_Vulture)))
		if ((him().Buildings(Terran_Command_Center).size() >= 2) ||
			((him().Buildings(Terran_Command_Center).size() == 1) &&
				!contains(ai()->GetMap().StartingLocations(), him().Buildings(Terran_Command_Center).front()->TopLeft())))
			return true;

	return false;
}


void TerranFastExpand::OnFrame_v()
{
	if (him().IsProtoss() || him().IsZerg()) return Discard();

	if (ai()->GetStrategy()->Detected<WraithRush>()) return Discard();
	if (him().Units(Terran_Wraith).size() >= 1) return Discard();

	if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 4) return Discard();

	if (m_detected)
	{
/*
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
*/
	}
	else
	{
		if (me().Buildings(Terran_Command_Center).size() >= 2) return Discard();
		if (him().Buildings(Terran_Factory).size() >= 2) return Discard();

		if (Detection())
		{
		///	ai()->SetDelay(100);
			m_detected = true;
			return;
		}
	}
}


} // namespace iron



