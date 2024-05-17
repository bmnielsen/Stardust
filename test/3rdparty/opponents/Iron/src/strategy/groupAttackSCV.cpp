//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "groupAttackSCV.h"
#include "../behavior/chasing.h"
#include "../behavior/fleeing.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/shallowTwo.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



static pair<int, string> computeGroupAttackSCVScore(HisBWAPIUnit * pTarget, const vector<pair<HisBWAPIUnit *, int>> & NeighboursWithDist,
													vector<MyUnit *> & Candidates)
{
	int countNeighbours2 = 0;
	int countNeighbours4 = 0;
	int countNeighbours6 = 0;
	int countNeighbours8 = 0;
	int countNeighbours10 = 0;
	int countNeighbours12 = 0;
	int dangerousNeighbours = 0;

	for (const auto & n : NeighboursWithDist)
	{
		int d = n.second;
		const HisBWAPIUnit * pHisNeighbour = n.first;

		if (pHisNeighbour->Is(Terran_Medic) && (d < 10*32))
			return make_pair(0, "-");

		if (!pHisNeighbour->Type().isWorker())
			++dangerousNeighbours;

		if (((pHisNeighbour->GroundRange() > 45) || (pHisNeighbour->GroundAttack() >= 10)) && d < 11*32)
			++dangerousNeighbours;

//		if (d <  2*32) if (++countNeighbours2  > 1) return make_pair(-1000, "-");
		if (d <  2*32) ++countNeighbours2;
		if (d <  4*32) ++countNeighbours4;
		if (d <  6*32) ++countNeighbours6;
		if (d <  8*32) ++countNeighbours8;
		if (d < 10*32) ++countNeighbours10;
		if (d < 12*32) ++countNeighbours12;
	}

	int distToMinerals = numeric_limits<int>::max();
	for (Mineral * m : pTarget->GetArea()->Minerals())
	{
		int d = distToRectangle(pTarget->Pos(), m->TopLeft(), m->Size());
		if (d < distToMinerals)
			distToMinerals = d;
	}

//	const Base * pMyBase = me().StartingBase();
	int bonusMiningDefense = 0;
/*
	if (pTarget->Is(Zerg_Zergling))
		if ((distToMinerals < 3*32))
			if (distToRectangle(pTarget->Pos(), pMyBase->Location(), UnitType(Terran_Command_Center).tileSize()) < 3*32)
			{
				int n = count_if(Candidates.begin(), Candidates.end(), [pTarget](const MyUnit * u)
							{ return dist(pTarget->Pos(), u->Pos()) < 5*32; });
				if (countNeighbours4 < n)
					bonusMiningDefense = 500;
			}
*/
	if (bonusMiningDefense == 0)
		if ((countNeighbours2  > 1) ||
			(countNeighbours4  > 2) ||
			(countNeighbours6  > 3) ||
			(countNeighbours8  > 4) ||
			(countNeighbours10 > 5) ||
			(countNeighbours12 > 6))
			return make_pair(0, "-");


	int bonusNearMinerals = pTarget->Type().isWorker() && !contains(me().EnlargedAreas(), pTarget->GetArea())
								? max(0, 45 - (max(12, distToMinerals) - 12)) : 0;
	int dangerousNeighboursMalus = 80*dangerousNeighbours;
	int neighboursMalus = 2*(5*countNeighbours2 + 4*countNeighbours4 + 3*countNeighbours6 + 2*countNeighbours8 + 1*countNeighbours10);
	int bloodScore = 3*(pTarget->MaxLifeWithShields() - pTarget->LifeWithShields());
	int targetStrengthMalus = pTarget->LifeWithShields() * pTarget->GroundAttack() / 10;
	int rangedTargetBonus = (pTarget->GroundRange() > 3*32) ? 100 : 0;				// ranged units are dangerous, kill them quick.
	int medicTargetBonus = pTarget->Is(Terran_Medic) ? 100 : 0;				// medic units are dangerous, kill them quick.


//	// do not group-chase single weak units
//	if ((pTarget->Type().isWorker() || pTarget->Is(Terran_Marine) || pTarget->Is(Zerg_Zergling)) &&
//		(neighboursMalus < 30))
//		return make_pair(-1000, "-");

	int canditatesBonus = 0;
	int score = 0;

	for (auto u : Candidates)
		canditatesBonus += u->Life() - u->Pos().getApproxDistance(pTarget->Pos()) * 3 / 32;

	score = bloodScore + canditatesBonus + rangedTargetBonus + medicTargetBonus + bonusNearMinerals + bonusMiningDefense
			- targetStrengthMalus - dangerousNeighboursMalus - neighboursMalus;

	if (Candidates.size() < 2) return make_pair(-1000, "-");

	string scoreDetail	=
					to_string(score) +
					" = blood: " + to_string(bloodScore) +
					"  friends: " + to_string(canditatesBonus) +
					" killRanged: " + to_string(rangedTargetBonus) +
					" nearMinerals: " + to_string(bonusNearMinerals) +
					" killMedic: " + to_string(medicTargetBonus) +
					" miningDefense: " + to_string(bonusMiningDefense) +
					"  -  targetStrength: " + to_string(targetStrengthMalus) +
					" dangerousEnemies: " + to_string(dangerousNeighboursMalus) +
					" enemies: " + to_string(neighboursMalus) +
					" // " + to_string(Candidates.size()) +
					" / " + to_string(NeighboursWithDist.size());

	return make_pair(score, scoreDetail);
}



void groupAttackSCV()
{
	vector<MyUnit *> SelectedCandidates;

	const int groupAttackSCV_threshold = 40;

	map<HisBWAPIUnit *, vector<MyUnit *>> map_Target_Candidates;
	for (auto & u : him().Units())
		if (!u->InFog())
			if (!u->Flying())
			//if (!((u->GetArea() == me().GetArea()) && u->Type().isWorker()))
			//if ((pArea == him().StartingBase()->GetArea()) || !faceOff.His()->Type().isBuilding())
			//if (u->Type().isWorker() || u->Is(Zerg_Zergling) || u->Is(Terran_Marine))
	//				if (u->Chasers().empty())
					if (u->Chasers().empty() || !u->Chasers().front()->NoMoreChasers())
						if (!(u->IsHisUnit() && !u->IsHisUnit()->MinesTargetingThis().empty()))
						{
/*
							//if (u->Is(Zerg_Zergling)) continue;
							// do not chase the enemy scout:
							if (u->Type().isWorker())
								if (const EnemyScout * pEnemyScout = ai()->GetStrategy()->Detected<EnemyScout>())
									if (u.get() == pEnemyScout->Get())
										continue;
*/
							map_Target_Candidates[u.get()].reserve(5);
						}

	vector<MyUnit *> Candidates;
	for (MyUnit * u : me().Units())
		if (u->Is(Terran_SCV))
			if (u->Completed() && !u->Loaded())
				if (u->AllowedToChase())
					if (u->Life() >= 30)
	//					if (!u->GetBehavior()->IsScouting())
						if (!u->GetBehavior()->IsConstructing())

						if (!(ai()->GetStrategy()->Has<ZerglingRush>() && !ai()->GetStrategy()->Detected<ZerglingRush>() && !u->GetStronghold()))
						if (!(ai()->GetStrategy()->Detected<ZerglingRush>() &&
							 (groundDist(u->Pos(), me().StartingBase()->Center()) < 8*32)))
						if (!(ai()->GetStrategy()->Detected<ZealotRush>() &&
							 (groundDist(u->Pos(), me().StartingBase()->Center()) < 30*32)))
//						if (!(ai()->GetStrategy()->Has<ShallowTwo>() &&
//							 (groundDist(u->Pos(), me().StartingBase()->Center()) < 8*32)))
	//						if (!u->GetBehavior()->IsChasing() ||
	//							((ai()->Frame() - u->GetBehavior()->IsChasing()->LastFrameTouchedHim() > 60) &&
	//							(u->GetBehavior()->IsChasing()->Target()->LifeWithShields() > 15)))
						if (!(u->GetBehavior()->IsDefaultBehavior() && ai()->GetStrategy()->Detected<ZerglingRush>()))
						if (!u->GetBehavior()->IsWalking())
						if (!u->GetBehavior()->IsExecuting())
						if (!u->GetBehavior()->IsDropping())
						if (!u->GetBehavior()->IsBlocking())
						if (!u->GetBehavior()->IsRepairing())
						if (!(u->GetBehavior()->IsChasing() && u->GetBehavior()->IsChasing()->WorkerDefense()))
						if (!u->GetBehavior()->IsFleeing() ||
							(u->GetBehavior()->IsFleeing()->State() != Fleeing::dragOut) &&
								((ai()->Frame() - u->GetBehavior()->IsFleeing()->InStateSince() < 5) ||
								(!u->GetBehavior()->IsFleeing()->CanReadAvgPursuers() ||	(u->GetBehavior()->IsFleeing()->AvgPursuers() < 2.2)))
							)
							Candidates.push_back(u);

	for (MyUnit * u : Candidates)
	{
		bool nearDangerousBuilding = false;
		for (const FaceOff & faceOff : u->FaceOffs())
			if (faceOff.His()->IsHisBuilding() || faceOff.His()->Is(Terran_Siege_Tank_Siege_Mode))
				if (faceOff.DistanceToHisRange() < 5*32) { nearDangerousBuilding = true; break; }
		if (nearDangerousBuilding) continue;

		for (const FaceOff & faceOff : u->FaceOffs())
		{
			// Do not push in map_Target_Candidates candidates already chasing Target:
//				if (u->GetBehavior()->IsChasing() && u->GetBehavior()->IsChasing()->Target() == faceOff.His()) continue;

			// Do not push in map_Target_Candidates candidates already chasing with success another target.
			if (u->GetBehavior()->IsChasing() && (u->GetBehavior()->IsChasing()->Target() != faceOff.His()))
				if (any_of(u->GetBehavior()->IsChasing()->Target()->Chasers().begin(), u->GetBehavior()->IsChasing()->Target()->Chasers().end(),
					[](Chasing * pChaser){ return pChaser->TouchedHim() && (ai()->Frame() - pChaser->LastFrameTouchedHim() < 30); }))
					continue;

			// WorkerDefense's job
			if (u->GetStronghold())
				if (!(u->GetBehavior()->IsChasing() && !u->GetBehavior()->IsChasing()->WorkerDefense()))
					if (groundRange(faceOff.His()->Type(), him().Player()) < 2*32)
						continue;

			auto it = map_Target_Candidates.find(faceOff.His());
			if (it == map_Target_Candidates.end()) continue;

			if (faceOff.DistanceToMyRange() > 8*32) continue;
			if (!faceOff.MyAttack()) continue;
			if (!faceOff.HisAttack() && !faceOff.His()->Is(Terran_Medic)) continue;
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
		int minChasers = (it.first->Type().isWorker() ? 2 :
						it.first->Is(Terran_Marine) ? 2 :
						it.first->Is(Zerg_Zergling) ? 2 :
						it.first->Is(Protoss_Zealot) ? 6 :
						min(10, it.first->LifeWithShields()/20));

		if ((int)it.second.size() >= minChasers)
		{
			if (it.second.size() == 2)
				if (it.second[0]->GetBehavior()->IsFleeing() || it.second[1]->GetBehavior()->IsFleeing())
					continue;

			map_Target_Candidates2[it.first] = it.second;
		}
	}

	map<HisBWAPIUnit *, vector<pair<HisBWAPIUnit *, int>>> map_Target_NeighboursWithDist;

	for (auto a : map_Target_Candidates2)
		for (auto & n : him().Units())
			if (n->GroundAttack() || n->Is(Terran_Medic))
				if (a.first != n.get())
				{
				//	int d = a.first->Pos().getApproxDistance(n->Pos());
				//	if (n->GroundRange() < 3*32)
				//		if (n->GetArea() != a.first->GetArea())
				//			d = groundDist(a.first->Pos(), n->Pos());
					int d = groundDist(a.first->Pos(), n->Pos());

					if (d < 12*32)
						map_Target_NeighboursWithDist[a.first].emplace_back(n.get(), d);
				}

	multimap<int, pair<HisBWAPIUnit *, string>> OrderedTargets;

	for (auto a : map_Target_Candidates2)
	{
		const vector<pair<HisBWAPIUnit *, int>> & NeighboursWithDist = map_Target_NeighboursWithDist[a.first];
		auto score = computeGroupAttackSCVScore(a.first, NeighboursWithDist, a.second);
		if (score.first >= groupAttackSCV_threshold)
			OrderedTargets.emplace(score.first, make_pair(a.first, score.second));
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
				bool a_chases_target = a->GetBehavior()->IsChasing() && (a->GetBehavior()->IsChasing()->Target() == pTarget);
				bool b_chases_target = b->GetBehavior()->IsChasing() && (b->GetBehavior()->IsChasing()->Target() == pTarget);
				if (a_chases_target && !b_chases_target) return true;
				if (b_chases_target && !a_chases_target) return false;

				return a->Pos().getApproxDistance(pTarget->Pos()) < b->Pos().getApproxDistance(pTarget->Pos());
			});

		int maxChasers = (pTarget->Type().isWorker() ? 3 :
						pTarget->Is(Terran_Marine) ? 4 :
						pTarget->Is(Zerg_Zergling) ? 4 :
						min(10, pTarget->LifeWithShields()/10));

		if (pTarget->GetArea() == me().GetArea())
			if (pTarget->Type().isWorker())
				if (pTarget->Is(Terran_SCV)) 	maxChasers = 2;
				else							maxChasers = 1;

		vector<MyUnit *> NewChasers;
		int i = 0;
		for (auto u : candidates)
		{
			if (!(u->GetBehavior()->IsChasing() && (u->GetBehavior()->IsChasing()->Target() == pTarget)))
				if ((int)pTarget->Chasers().size() < maxChasers)
				{
					u->ChangeBehavior<Chasing>(u, it->second.first);
					NewChasers.push_back(u);
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
//					string caption = to_string(score);
			bw->drawTextMap(pTarget->Pos() - Position(30, 42), "%c%s", Text::Cyan, caption.c_str());
					
			if (!NewChasers.empty())
			{
//				bw << ai()->Frame() << ") " << score << " group-attack " << pTarget->NameWithId();
				for (auto u : candidates)
					bw << (contains(NewChasers, u) ? "  (+)" : "  ") << u->Unit()->getID();
				bw << endl; ai()->SetDelay(500);
			}
		}
*/
	}
}




} // namespace iron



