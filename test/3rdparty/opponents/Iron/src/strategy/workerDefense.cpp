//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "workerDefense.h"
#include "strategy.h"
#include "walling.h"
#include "workerRush.h"
#include "zealotRush.h"
#include "../units/him.h"
#include "../units/my.h"
#include "../units/cc.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/mining.h"
#include "../behavior/exploring.h"
#include "../behavior/constructing.h"
#include "../behavior/fleeing.h"
#include "../behavior/chasing.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class WorkerDefense
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


WorkerDefense::WorkerDefense()
{
}


WorkerDefense::~WorkerDefense()
{
//	ReleaseExplorers();
}


string WorkerDefense::StateDescription() const
{
	if (!Active()) return "-";
	if (Active()) return "active";

	return "-";
}


void WorkerDefense::OnBWAPIUnitDestroyed(BWAPIUnit * )
{
}


bool WorkerDefense::Active() const
{
	return count_if(Chasing::Instances().begin(), Chasing::Instances().end(), [](Chasing * chaser)
						{ return chaser->WorkerDefense(); }) >= 1;
}


void WorkerDefense::OnFrame_v()
{
	if (ai()->GetStrategy()->Detected<ZealotRush>()) return;
//	if (him().IsProtoss() && (me().Bases().size() < 3)) return;

	MyUnit * pAttackedSCV = nullptr;
	vector<MyUnit *> ChasingCandidates;

	for (const auto & u : me().Units(Terran_SCV))
		if (u->Completed() && !u->Loaded())
			if (u->GetStronghold())
				if (!u->FaceOffs().empty())
				{
					if (u->Life() < u->PrevLife(10))
//							if (findThreatsButWorkers(u.get(), 3*32).empty())
							pAttackedSCV = u.get();

					if (const Mining * m = u->GetBehavior()->IsMining())
						//if ((m->State() == Mining::returning) || u->Unit()->isMoving())
							ChasingCandidates.push_back(u.get());

					if (u->GetBehavior()->IsRefining())
						if (u->Unit()->isMoving())
							ChasingCandidates.push_back(u.get());

					if (u->GetBehavior()->IsConstructing())
						ChasingCandidates.push_back(u.get());

					if (u->GetBehavior()->IsSupplementing())
						ChasingCandidates.push_back(u.get());

					if (u->GetBehavior()->IsRepairing())
						if (!ai()->GetStrategy()->Active<Walling>())
							ChasingCandidates.push_back(u.get());

//						if (const Constructing * c = u->GetBehavior()->IsConstructing())
//							if (c->State() == Constructing::reachingLocation )
//								ChasingCandidates.push_back(u.get());
				}


	for (MyUnit * u : ChasingCandidates)
	{
		bool weAreAttackedHere = any_of(ChasingCandidates.begin(), ChasingCandidates.end(), [u](const MyUnit * attacked)
									{ return (attacked->Life() < attacked->PrevLife(10)) && (dist(u->Pos(), attacked->Pos()) < 50); });

		int minDistToMyRange = 3*32;
		const FaceOff * pNearestFaceOff = nullptr;
		for (const auto & fo : u->FaceOffs())
			if (fo.MyAttack() && fo.HisAttack())
			{
				if (fo.DistanceToMyRange() < minDistToMyRange)
				{
					minDistToMyRange = fo.DistanceToMyRange();
					pNearestFaceOff = &fo;
				}
			}


		if (pNearestFaceOff)
			if (HisUnit * pHisUnit = pNearestFaceOff->His()->IsHisUnit())
				if (groundRange(pHisUnit->Type(), him().Player()) < 3*32)
				{
					if (!pHisUnit->Type().isWorker() ||
						(pAttackedSCV == u) ||
						(pNearestFaceOff->DistanceToMyRange() < (weAreAttackedHere ? 40 : 10)))
					{
						int maxChasingTime = 18 + pNearestFaceOff->DistanceToMyRange()/4;

						if (u->GetBehavior()->IsConstructing())
							if (pAttackedSCV == u)
							{
								maxChasingTime = 1000;

								if (!pHisUnit->Chasers().empty()) return;

								if (pHisUnit->Chasers().empty())
									if (auto s = ai()->GetStrategy()->Active<Walling>())
										if (contains(s->GetWall()->Locations(), u->GetBehavior()->IsConstructing()->Location()))
										{

											if (!pHisUnit->Type().isWorker())
												return;

										//	if (My<Terran_SCV> * pWorker = findFreeWorker_urgent(me().StartingVBase()))
										//		return pWorker->ChangeBehavior<Chasing>(pWorker, pHisUnit, bool("insist"), 10000);

											maxChasingTime = 10000;
										}
							}
							else return;

						u->ChangeBehavior<Chasing>(u, pHisUnit, bool("insist"), maxChasingTime, bool("workerDefense"));
					}
				}
	}
}


} // namespace iron



