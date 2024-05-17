//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "groupAttack.h"
#include "../behavior/vchasing.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



struct GroupTargetInfo
{
	GroupTargetInfo(int minVultures) : minVultures(minVultures) {}
	int minVultures;
};

const map<UnitType, GroupTargetInfo> GroupTargetInfoMap =
{
	{Terran_Goliath, 3},
	{Terran_Vulture, 2},
	{Terran_Siege_Tank_Tank_Mode, 4},
	{Terran_Siege_Tank_Siege_Mode, 4},
	{Protoss_Dragoon, 4},
	{Protoss_Reaver, 6},
	{Zerg_Hydralisk, 2},
};



static pair<int, string> computeGroupAttackScore(HisBWAPIUnit * pTarget, vector<MyUnit *> & Candidates)
{
	if (Candidates.size() < 2) return make_pair(-1000, "-");

	int minVultures = GroupTargetInfoMap.find(pTarget->Type())->second.minVultures;
	minVultures = 1 + minVultures * (pTarget->LifeWithShields()-1) / pTarget->MaxLifeWithShields();
	int maxVultures = 2*minVultures;

	int bloodScore = 3*(pTarget->MaxLifeWithShields() - pTarget->LifeWithShields());

	int sumCandidatesLife = 0;
	int sumCandidatesMaxLife = 0;
	int n = maxVultures;
	for (MyUnit * u : Candidates)
	{
		sumCandidatesLife += u->Life();
		sumCandidatesMaxLife += u->MaxLife();
		if (--n == 0) break;
	}

	int manyBonus = 200 * min(maxVultures, (int)Candidates.size()) / minVultures - 200;
	int damagedCandidatesMalus = 200 - 200 * sumCandidatesLife / sumCandidatesMaxLife;

	int score = bloodScore + manyBonus
		- damagedCandidatesMalus;

	string scoreDetail	=
					to_string(score) +
					" = blood: " + to_string(bloodScore) +
					"  manyBonus: " + to_string(manyBonus) +
					"  -  damagedCandidatesMalus: " + to_string(damagedCandidatesMalus);

	return make_pair(score, scoreDetail);
}



void groupAttack()
{
	if (him().IsTerran()) return;
	if (him().IsProtoss()) return;
	if (him().IsZerg()) return;

	vector<MyUnit *> SelectedCandidates;

	const int groupAttack_threshold = -40;

	map<HisBWAPIUnit *, vector<MyUnit *>> map_Target_Candidates;
	for (auto & u : him().Units())
		if (!u->InFog())
			//if (!((u->GetArea() == me().GetArea()) && u->Type().isWorker()))
			//if ((pArea == him().StartingBase()->GetArea()) || !faceOff.His()->Type().isBuilding())
			if (GroupTargetInfoMap.find(u->Type()) != GroupTargetInfoMap.end())
	//			if (u->VChasers().empty())
					if (u->VChasers().empty() || !u->VChasers().front()->NoMoreVChasers())
						if (!(u->IsHisUnit() && !u->IsHisUnit()->MinesTargetingThis().empty()))
							map_Target_Candidates[u.get()].reserve(5);

	vector<MyUnit *> Candidates;
	for (MyUnit * u : me().Units())
		if (u->Is(Terran_Vulture))
			if (u->Completed() && !u->Loaded())
	//			if (u->AllowedToChase())
					if ((u->Life() > u->MaxLife()*3/4) || !u->WorthBeingRepaired())
						if (!u->GetBehavior()->IsKillingMine())
						if (!u->GetBehavior()->IsDestroying())
						if (!u->GetBehavior()->IsFighting())
	//					if (!u->GetBehavior()->IsFleeing() ||
	//						(u->GetBehavior()->IsFleeing()->State() != Fleeing::dragOut) &&
	//							((ai()->Frame() - u->GetBehavior()->IsFleeing()->InStateSince() < 5) ||
	//							(!u->GetBehavior()->IsFleeing()->CanReadAvgPursuers() ||	(u->GetBehavior()->IsFleeing()->AvgPursuers() < 2.2)))
	//						)
							Candidates.push_back(u);

	for (MyUnit * u : Candidates)
	{
		bool nearStaticThreat = false;
		for (const FaceOff & faceOff : u->FaceOffs())
		{
			assert_throw(faceOff.His());
			if (faceOff.His()->IsHisBuilding())
				if (faceOff.DistanceToHisRange() < 5*32) { nearStaticThreat = true; break; }

			if (faceOff.His()->Is(Zerg_Lurker) && faceOff.His()->Burrowed())
				if (faceOff.DistanceToHisRange() < 5*32) { nearStaticThreat = true; break; }

			if (faceOff.His()->Is(Terran_Siege_Tank_Siege_Mode))
				if (faceOff.DistanceToHisRange() < 5*32) { nearStaticThreat = true; break; }
		}
		if (nearStaticThreat) continue;

		for (const FaceOff & faceOff : u->FaceOffs())
		{
			// Do not push in map_Target_Candidates candidates already chasing Target:
//				if (u->GetBehavior()->IsVChasing() && u->GetBehavior()->IsVChasing()->Target() == faceOff.His()) continue;

			// Do not push in map_Target_Candidates candidates already chasing with success another target.
			if (u->GetBehavior()->IsVChasing() && (u->GetBehavior()->IsVChasing()->Target() != faceOff.His()))
				if (any_of(u->GetBehavior()->IsVChasing()->Target()->VChasers().begin(), u->GetBehavior()->IsVChasing()->Target()->VChasers().end(),
					[](VChasing * pVChaser){ return pVChaser->TouchedHim() && (ai()->Frame() - pVChaser->LastFrameTouchedHim() < 30); }))
					continue;

			auto it = map_Target_Candidates.find(faceOff.His());
			if (it == map_Target_Candidates.end()) continue;

			if (faceOff.DistanceToMyRange() > 10*32) continue;
			if (!faceOff.MyAttack()) continue;
			if (!faceOff.HisAttack()/* && !faceOff.His()->Is(Terran_Medic)*/) continue;
			assert_throw(!contains(it->second, u));

//				if (ai()->GetVMap().NearBuildingOrNeutral(ai()->GetMap().GetTile(TilePosition((u->Pos() + faceOff.His()->Pos()) / 2))))
//					continue;

			it->second.push_back(u);
		}
	}

	// only keep Targets with enough candidates to make a group of chasers
	map<HisBWAPIUnit *, vector<MyUnit *>> map_Target_Candidates2;
	for (auto it : map_Target_Candidates)
	{
		int minVultures = GroupTargetInfoMap.find(it.first->Type())->second.minVultures;
		minVultures = 1 + minVultures * (it.first->LifeWithShields()-1) / it.first->MaxLifeWithShields();

		if ((int)it.second.size() >= minVultures)
		{
/*
			if (it.second.size() == 2)
				if (it.second[0]->GetBehavior()->IsFleeing() || it.second[1]->GetBehavior()->IsFleeing())
					continue;
*/

			map_Target_Candidates2[it.first] = it.second;
		}
	}

	map<HisBWAPIUnit *, vector<pair<HisBWAPIUnit *, int>>> map_Target_NeighboursWithDist;

	multimap<int, pair<HisBWAPIUnit *, string>> OrderedTargets;

	for (auto & a : map_Target_Candidates2)
	{
		bool enemyNotAlone = false;
		for (auto & n : him().Units())
			if (n->GroundAttack())
				if (!n->Type().isWorker())
					if (a.first != n.get())
					{
					//	int d = a.first->Pos().getApproxDistance(n->Pos());
					//	if (n->GroundRange() < 3*32)
					//		if (n->GetArea() != a.first->GetArea())
					//			d = groundDist(a.first->Pos(), n->Pos());
						int d = groundDist(a.first->Pos(), n->Pos());

						if (d < n->GroundRange() + 3*32) { enemyNotAlone = true; break; }
					}
		if (enemyNotAlone) continue;

		for (auto & n : him().Buildings())
			if (n->GroundAttack())
			{
				int d = a.first->Pos().getApproxDistance(n->Pos());
				if (d < n->GroundRange() + 3) { enemyNotAlone = true; break; }
			}
		if (enemyNotAlone) continue;

		{
			auto score = computeGroupAttackScore(a.first, a.second);
			if (score.first >= groupAttack_threshold)
				OrderedTargets.emplace(score.first, make_pair(a.first, score.second));
		}
	}

	for (auto it = OrderedTargets.crbegin() ; it != OrderedTargets.crend() ; ++it)
	{
		const int score = it->first;
		::unused(score);
		HisBWAPIUnit * pTarget = it->second.first;
		auto & candidates = map_Target_Candidates2[it->second.first];
			
		// group-attacking is exclusive:
		if (any_of(candidates.begin(), candidates.end(), [&SelectedCandidates](MyUnit * u){ return contains(SelectedCandidates, u); }))
			continue;

		sort(candidates.begin(), candidates.end(), [pTarget](MyUnit * a, MyUnit * b)
			{
				bool a_chases_target = a->GetBehavior()->IsVChasing() && (a->GetBehavior()->IsVChasing()->Target() == pTarget);
				bool b_chases_target = b->GetBehavior()->IsVChasing() && (b->GetBehavior()->IsVChasing()->Target() == pTarget);
				if (a_chases_target && !b_chases_target) return true;
				if (b_chases_target && !a_chases_target) return false;

				return a->Pos().getApproxDistance(pTarget->Pos()) < b->Pos().getApproxDistance(pTarget->Pos());
			});

		int minVultures = GroupTargetInfoMap.find(pTarget->Type())->second.minVultures;
		minVultures = 1 + minVultures * (pTarget->LifeWithShields()-1) / pTarget->MaxLifeWithShields();
		int maxVultures = minVultures*2;
/*							(pTarget->Type().isWorker() ? 3 :
						pTarget->Is(Terran_Marine) ? 4 :
						pTarget->Is(Zerg_Zergling) ? 4 :
						min(10, pTarget->LifeWithShields()/10));*/


		vector<MyUnit *> NewVChasers;
		int i = 0;
		for (auto u : candidates)
		{
			if (!(u->GetBehavior()->IsVChasing() && (u->GetBehavior()->IsVChasing()->Target() == pTarget)))
				if ((int)pTarget->VChasers().size() < maxVultures)
				{
					u->ChangeBehavior<VChasing>(u, it->second.first);
					NewVChasers.push_back(u);
				}
				else break;

			SelectedCandidates.push_back(u);
			++i;
		}
		candidates.resize(i);

/*
		{
			for (int r = 29 ; r <= 30 ; ++r)
				bw->drawBoxMap(pTarget->Pos() - r, pTarget->Pos() + r, Colors::Cyan);
			string caption = it->second.second;
//			string caption = to_string(score);
			bw->drawTextMap(pTarget->Pos() - Position(30, 42), "%c%s", Text::Cyan, caption.c_str());
					
			if (!NewVChasers.empty())

			{
				bw << ai()->Frame() << ") " << score << " group-attack " << pTarget->NameWithId();
				for (auto u : candidates)
					bw << (contains(NewVChasers, u) ? "  (+)" : "  ") << u->Unit()->getID();
				bw << endl; ai()->SetDelay(1000);
			}
		}
*/
	}
}




} // namespace iron



