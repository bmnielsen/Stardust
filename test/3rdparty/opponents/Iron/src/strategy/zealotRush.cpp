//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "zealotRush.h"
#include "../units/army.h"
#include "../behavior/mining.h"
#include "../behavior/chasing.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ZealotRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


ZealotRush::ZealotRush()
{
}


string ZealotRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


void ZealotRush::WorkerDefense()
{
	for (const auto & u : me().Units(Terran_SCV))
		if (u->Completed() && !u->Loaded())
			if (groundDist(u->Pos(), me().StartingBase()->Center()) < 10*32)
				if (u->GetBehavior()->IsMining() ||
					u->GetBehavior()->IsRefining())
				{
					int minDistToMyRange = 3*32;
					HisUnit * pNearestTarget = nullptr;
					for (const auto & faceOff : u->FaceOffs())
						if (faceOff.MyAttack() && faceOff.HisAttack())
						if (faceOff.DistanceToMyRange() < minDistToMyRange)
						{
							minDistToMyRange = faceOff.DistanceToMyRange();
							pNearestTarget = faceOff.His()->IsHisUnit();
						}

					if (pNearestTarget)
						u->ChangeBehavior<Chasing>(u.get(), pNearestTarget, bool("insist"), 15 + minDistToMyRange/4);
				}
}


bool ZealotRush::Detection() const
{
	int hisZealotsHere = 0;
	for (const auto & u : him().Units())
		if (u->Is(Protoss_Zealot))
			if (groundDist(u->Pos(), me().StartingBase()->Center()) < 30*32)
				++hisZealotsHere;


	return (hisZealotsHere >= 1) && (me().CompletedUnits(Terran_Marine) < hisZealotsHere + 2);
}


void ZealotRush::OnFrame_v()
{
	if (him().IsTerran() || him().IsZerg()) return Discard();
	if (me().CompletedUnits(Terran_Marine) > 8) return Discard();

	if (m_detected)
	{
		if ((me().CompletedUnits(Terran_Marine) > 8) && !Detection())
			return Discard();

		WorkerDefense();

		for (const auto & b : me().Buildings(Terran_Refinery))
			if (!b->Completed())
				if (b->CanAcceptCommand())
					DO_ONCE
					{
					///	bw << "cancel Refinery" << endl;
					///	ai()->SetDelay(500);
						return b->CancelConstruction();
					}

		for (const auto & b : me().Buildings(Terran_Academy))
			if (!b->Completed())
				if (b->CanAcceptCommand())
					DO_ONCE
					{
					///	bw << "cancel Academy" << endl;
					///	ai()->SetDelay(500);
						return b->CancelConstruction();
					}

/*
		if (Mining::Instances().size() < 6)
			if (me().MineralsAvailable() < 30)
				if (!(me().UnitsBeingTrained(Terran_Vulture) && me().UnitsBeingTrained(Terran_Marine)))
				{

					if (me().SupplyAvailable() >= 3)
						for (const auto & b : me().Buildings(Terran_Supply_Depot))
							if (!b->Completed())
								if (b->CanAcceptCommand())
									return b->CancelConstruction();

					if (me().Buildings(Terran_Factory).size() >= 2)
						if (me().Units(Terran_Vulture).size() == 0)
						{
							MyBuilding * pLatestUncompletedFactory = nullptr;
							for (const auto & b : me().Buildings(Terran_Factory))
								if (!b->Completed())
									if (!pLatestUncompletedFactory || (b->RemainingBuildTime() > pLatestUncompletedFactory->RemainingBuildTime()))
										pLatestUncompletedFactory = b.get();

							if (pLatestUncompletedFactory)
								if (pLatestUncompletedFactory->RemainingBuildTime() > 750)
									if (pLatestUncompletedFactory->CanAcceptCommand())
										return pLatestUncompletedFactory->CancelConstruction();
						}
				}
*/
/*
		static frame_t lastCancel = 0;
///		bw << ai()->Frame() - lastCancel << endl;
		if (ai()->Frame() - lastCancel > 3*bw->getRemainingLatencyFrames())
			for (UnitType type : {Terran_Vulture, Terran_Marine})
				for (const auto & bb : me().Buildings(type == Terran_Marine ? Terran_Barracks : Terran_Factory))
					if (bb->Completed() && bb->CanAcceptCommand() && !bb->Unit()->isTraining())
//						if ((type != Terran_Marine) || ((int)me().Units(Terran_Marine).size() < MaxMarines()))
						{
							const bool needMinerals = me().MineralsAvailable() - Cost(type).Minerals() < -3*(int)Mining::Instances().size();
							const int supplyResult = me().SupplyAvailable() - Cost(type).Supply();

							if (( needMinerals && (supplyResult + 1 >= 0)) ||
								(!needMinerals && (supplyResult + 1 == 0)))
								for (const auto & b : me().Buildings(Terran_Command_Center))
									if (b->Unit()->isTraining())
										if (b->TimeToTrain() > 75)
											if (b->CanAcceptCommand())
											{
											///	bw << "CancelTrain SCV" << endl;
											///	ai()->SetDelay(5000);
												lastCancel = ai()->Frame();
												return b->CancelTrain();
											}

							if (needMinerals && (supplyResult >= 0))
								for (const auto & b : me().Buildings(Terran_Supply_Depot))
									if (!b->Completed())
										if (b->CanAcceptCommand())
										{
										///	bw << "CancelConstruction Depot" << endl;
										///	ai()->SetDelay(5000);
											lastCancel = ai()->Frame();
											return b->CancelConstruction();
										}
						}
*/
	}
	else
	{
		if (me().CompletedUnits(Terran_Vulture) >= 2) return Discard();

		if (Detection())
		{
		///	ai()->SetDelay(100);
			m_detected = true;
			return;
		}
	}
}


} // namespace iron



