//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "scan.h"
#include "../behavior/behavior.h"
#include "../behavior/sieging.h"
#include "../units/factory.h"
#include "../units/comsat.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


bool isWorthScaning(HisUnit * target, const vector<MyUnit *> & Candidates)
{
	if (Candidates.size() >= 5)
		return true;

	if (Candidates.size() >= 3)
		if (!target->Is(Protoss_Dark_Templar))
			if (any_of(Candidates.begin(), Candidates.end(), [](const MyUnit * u)
					{ return u->Is(Terran_Siege_Tank_Siege_Mode) || u->Is(Terran_Wraith); }))
				return true;

	if (Candidates.size() >= 1)
		if (target->Is(Protoss_Observer))
			if (any_of(Candidates.begin(), Candidates.end(), [](const MyUnit * u)
					{ return u->Is(Terran_Goliath) || u->Is(Terran_Wraith) || u->Is(Terran_Valkyrie); }))
				return true;

	return false;
}


static frame_t sScanLastTime = 0;

frame_t lastScan()
{
	return sScanLastTime;
}

void scan()
{
	if (ai()->Frame() - sScanLastTime < 100) return;

	if (!(me().BestComsat() && me().BestComsat()->CanAcceptCommand())) return;

	struct TargetInfo
	{
		vector<MyUnit *>	CloseCandidates;
		vector<MyUnit *>	NotTooFarCandidates;
	};
	
	map<HisUnit *, TargetInfo> map_Target_Candidates;
	for (const auto & u : him().Units())
		if (u->Is(Zerg_Lurker) ||
			u->Is(Protoss_Dark_Templar) ||
			u->Is(Protoss_Observer) && (!him().MayDarkTemplar() || him().MayDarkTemplar() && him().AllUnits(Protoss_Dark_Templar).empty() && (me().TotalAvailableScans() >= 3)) ||
			u->Is(Terran_Wraith))
			if (u->InFog() || !u->Unit()->isDetected())
			{
				map_Target_Candidates[u.get()].CloseCandidates.reserve(5);
				map_Target_Candidates[u.get()].NotTooFarCandidates.reserve(10);
			}

	vector<MyUnit *> Candidates;
	for (MyUnit * u : me().Units())
		if (u->Completed() && !u->Loaded())
			if ( ((u->Is(Terran_Vulture) || u->Is(Terran_Wraith) || u->Is(Terran_Valkyrie) || u->Is(Terran_Marine) || u->Is(Terran_Goliath))
					&& (u->GetBehavior()->IsKiting() ||
						u->GetBehavior()->IsFighting() ||
						u->GetBehavior()->IsExploring() ||
						u->GetBehavior()->IsRaiding() ||
						u->GetBehavior()->IsRepairing()))
				||(u->Is(Terran_Siege_Tank_Siege_Mode)
					&& (u->GetBehavior()->IsSieging() && (u->GetBehavior()->IsSieging()->State() != Sieging::unsieging)))
				)
				if (u->Is(Terran_Siege_Tank_Siege_Mode) || (u->NotFiringFor() > 2*u->AvgCoolDown()))
					Candidates.push_back(u);

	for (MyUnit * u : Candidates)
		for (auto & tc : map_Target_Candidates)
		{
			if (u->Is(Terran_Siege_Tank_Siege_Mode))
			{
				if (!dynamic_cast<const My<Terran_Siege_Tank_Tank_Mode> *>(u)->CanSiegeAttack(tc.first))
					continue;
			}
			else
			{
				if (!u->Flying() && !tc.first->Flying() && (groundDist(u->Pos(), tc.first->Pos()) > 15*32)) continue;

				int closeLimit;
				if (tc.first->Is(Protoss_Observer) || tc.first->Is(Terran_Wraith))
				{
					if (!u->AirAttack()) continue;

					closeLimit = u->Is(Terran_Wraith) ? 2*32 : 0*32;
				}
				else
				{
					closeLimit = (u->Is(Terran_Vulture) || u->Is(Terran_Wraith) ? 10*32 : u->Is(Terran_Marine) ? 1*32 : 5*32);
				}

				int notTooFarLimit = closeLimit + 3*32;

				if (FaceOff(u, tc.first).DistanceToMyRange() <= notTooFarLimit)
				{
					tc.second.NotTooFarCandidates.push_back(u);
					
					if (FaceOff(u, tc.first).DistanceToMyRange() <= closeLimit)
						tc.second.CloseCandidates.push_back(u);
				}

			}
		}

	for (const auto & tc : map_Target_Candidates)
	{
#if DEV
		for (int i = 0 ; i < (int)tc.second.NotTooFarCandidates.size() ; ++i)
		{
			bw->drawBoxMap(tc.first->Pos()-(30+2*i), tc.first->Pos()+(30+2*i), Colors::Yellow);
			drawLineMap(tc.second.NotTooFarCandidates[i]->Pos(), tc.first->Pos(), Colors::Yellow);
		}
		for (int i = 0 ; i < (int)tc.second.CloseCandidates.size() ; ++i)
		{
			bw->drawBoxMap(tc.first->Pos()-(30+2*i), tc.first->Pos()+(30+2*i), Colors::Orange);
			drawLineMap(tc.second.CloseCandidates[i]->Pos(), tc.first->Pos(), Colors::Orange);
		}
#endif

		if (isWorthScaning(tc.first, tc.second.NotTooFarCandidates))
		{
			tc.first->OnWorthScaning();

			if (isWorthScaning(tc.first, tc.second.CloseCandidates))
			{
				me().BestComsat()->Scan(tc.first->Pos());
				sScanLastTime = ai()->Frame();
				return;
			}
		}
	}
}




} // namespace iron



