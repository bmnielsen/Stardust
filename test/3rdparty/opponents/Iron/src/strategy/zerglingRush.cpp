//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "zerglingRush.h"
#include "strategy.h"
#include "walling.h"
#include "../units/cc.h"
#include "../behavior/constructing.h"
#include "../behavior/mining.h"
#include "../behavior/fleeing.h"
#include "../behavior/walking.h"
#include "../behavior/chasing.h"
#include "../behavior/blocking.h"
#include "../behavior/sniping.h"
#include "../behavior/raiding.h"
#include "../behavior/supplementing.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ZerglingRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


ZerglingRush::ZerglingRush()
{
}


ZerglingRush::~ZerglingRush()
{
	ai()->GetStrategy()->SetMinScoutingSCVs(1);
}


string ZerglingRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected " + to_string(m_zerglings)
		+ (m_hypotheticZerglings ? " (" + to_string(m_hypotheticZerglings) + ")" : string(""))
		+  (TechRestartingCondition() ? " tech" : "");

	return "-";
}


static vector<Position> getChokePointDefensePositions(const ChokePoint * cp, int n)
{
    const Area * pDefenseArea =
		dist(center(cp->GetAreas().first->Top()), me().StartingBase()->Center()) <
		dist(center(cp->GetAreas().second->Top()), me().StartingBase()->Center())
		? cp->GetAreas().first
		: cp->GetAreas().second;

	vector<Position> List{center(cp->Center())};
	
	List.push_back(center(cp->Geometry().front()));
	List.push_back(center(cp->Geometry().back()));

	int i = 0;
	while ((int)List.size() != n)
	{
		auto startingNode = ChokePoint::node(i++ % ChokePoint::node_count);
		Position newPos = center(ai()->GetMap().BreadthFirstSearch(cp->PosInArea(startingNode, pDefenseArea),
			[pDefenseArea, &List](const MiniTile & miniTile, WalkPosition w)	// findCond
				{ return (miniTile.AreaId() == pDefenseArea->Id()) &&
						none_of(List.begin(), List.end(), [w](Position p) { return roundedDist(w, WalkPosition(p)) < 4; }) &&
						any_of(List.begin(), List.end(), [w](Position p) { return roundedDist(w, WalkPosition(p)) >= 4; }); },
			[pDefenseArea](const MiniTile & miniTile, WalkPosition)		// visitCond
				{ return (miniTile.AreaId() == pDefenseArea->Id()); }
			));

		List.push_back(newPos);
	}

	return List;
}

/*
static TilePosition findBunkerLocation(const ChokePoint * pDefenseCP)
{
	
		TilePosition location = center(ai()->GetMap().BreadthFirstSearch(cp->PosInArea(startingNode, pDefenseArea),
			[pDefenseArea, &List](const MiniTile & miniTile, WalkPosition w)	// findCond
				{ return (miniTile.AreaId() == pDefenseArea->Id()) &&
						none_of(List.begin(), List.end(), [w](Position p) { return roundedDist(w, WalkPosition(p)) < 4; }) &&
						any_of(List.begin(), List.end(), [w](Position p) { return roundedDist(w, WalkPosition(p)) >= 4; }); },
			[pDefenseArea](const MiniTile & miniTile, WalkPosition)		// visitCond
				{ return (miniTile.AreaId() == pDefenseArea->Id()); }
			));
}
*/

void ZerglingRush::ChokePointDefense()
{
	if (const ChokePoint * pDefenseCP = ai()->Me().GetVArea()->DefenseCP())
	{

		
//		DO_ONCE
//			m_BunkerLocation = findBunkerLocation(pDefenseCP);

		int zerglingsHere = count_if(him().AllUnits(Zerg_Zergling).begin(), him().AllUnits(Zerg_Zergling).end(), [pDefenseCP](const HisKnownUnit * u)
								{
									return (ai()->Frame() - u->LastTimeVisible() < 150) &&
											(dist(u->LastPosition(), center(pDefenseCP->Center())) < 25*32);
								});
		int defenders = min(9, 6 + max(2, zerglingsHere));

	///	bw << zerglingsHere << "  " << defenders << endl;


		MyUnit * pClosestMarine = nullptr;
		int minDistMarine = 32*10;
		for (const auto & u : me().Units(Terran_Marine))
			if (u->Completed() && !u->Loaded())
				if (dist(u->Pos(), center(pDefenseCP->Center())) < minDistMarine)
				{
					minDistMarine = roundedDist(u->Pos(), center(pDefenseCP->Center()));
					pClosestMarine = u.get();
				}

		vector<Position> DefensePositions = getChokePointDefensePositions(pDefenseCP, defenders+3);
		sort(DefensePositions.begin(), DefensePositions.end(), [pDefenseCP, pClosestMarine](Position a, Position b)
				{ return dist(a, center(pDefenseCP->Center())) + (pClosestMarine ? dist(a, pClosestMarine->Pos()) : 0) <
						 dist(b, center(pDefenseCP->Center())) + (pClosestMarine ? dist(b, pClosestMarine->Pos()) : 0); });

		{
			vector<Blocking *> MovedBlockers;
		///	int i = 0;
			for (Position p : DefensePositions)
			{
			///	bw->drawTextMap(p, "%c%d", Text::White, i++);
			///	bw->drawCircleMap(p, 3, Colors::White);

				if (any_of(Blocking::Instances().begin(), Blocking::Instances().end(), [p](const Blocking * b)
												{ return roundedDist(b->Agent()->Pos(), p) < 4; }))
					continue;

				int minDist = numeric_limits<int>::max();
				Blocking * pClosestCandidate = nullptr;
				for (Blocking * b : Blocking::Instances())
					if (!contains(MovedBlockers, b))
						if (b->State() == Blocking::blocking)
							//if (b->Agent()->Unit()->isIdle())
								if (roundedDist(b->Agent()->Pos(), p) < minDist)
							{
								minDist = roundedDist(b->Agent()->Pos(), p);
								pClosestCandidate = b;
							}

				if (!pClosestCandidate) break;

				MovedBlockers.push_back(pClosestCandidate);

				if (pClosestCandidate->Agent()->CanAcceptCommand())
				{
					pClosestCandidate->Agent()->Move(p);
				///	drawLineMap(pClosestCandidate->Agent()->Pos(), p, Colors::White);
				}
			}
		}
			


		int minDistToCC = numeric_limits<int>::max();
		Blocking * pBackEndBlocker = nullptr;

		for (Blocking * pBlocker : Blocking::Instances())
			if (pBlocker->State() == Blocking::blocking)
			{
				int d = roundedDist(pBlocker->Agent()->Pos(), me().StartingBase()->Center());
				if (d < minDistToCC)
				{
					minDistToCC = d;
					pBackEndBlocker = pBlocker;
				}
			}

		
		int marinesAroundBlockingPos = 0;
		Position marinesBlockingPos = 
			pBackEndBlocker && (dist(pBackEndBlocker->Agent()->Pos(), center(pDefenseCP->Center())) < 4*32)
			? pBackEndBlocker->Agent()->Pos()
			: center(pDefenseCP->Center());

		for (const auto & u : me().Units(Terran_Marine))
			if (u->Completed())
			{
				if ((me().CompletedBuildings(Terran_Bunker) >= 1) || dist(u->Pos(), marinesBlockingPos) < 32*8)
					++marinesAroundBlockingPos;

				if (!u->Loaded())
				{
					if (u->GetBehavior()->IsExploring())
					{
						if (dist(u->Pos(), me().StartingBase()->Center()) + 32*3 > dist(center(pDefenseCP->Center()), me().StartingBase()->Center()))
						{
							Position retraitingPos = (me().StartingBase()->Center() + marinesBlockingPos)/2;
							if (ai()->GetMap().GetArea(WalkPosition(retraitingPos)) != me().StartingBase()->GetArea())
								retraitingPos = center(me().StartingBase()->GetArea()->Top());

							u->ChangeBehavior<Raiding>(u.get(), retraitingPos);
						}

						if (me().CompletedBuildings(Terran_Bunker) == 0)
							if (dist(u->Pos(), marinesBlockingPos) > 32*6)
							{
								u->ChangeBehavior<Raiding>(u.get(), marinesBlockingPos);
							}
					}
				}
			}


//		int defenders = min(9, 6 + max(2, zerglingsHere));
//		m_blockersWanted = max(2, defenders - marinesAroundBlockingPos * (m_snipersAvailable ? 3 : 1));
		m_blockersWanted = defenders;
		if (m_zerglings > 10) m_blockersWanted = m_zerglings;

		if (marinesAroundBlockingPos)
		{
			if (m_snipersAvailable)
				m_blockersWanted -= marinesAroundBlockingPos * (me().CompletedBuildings(Terran_Bunker) >= 2 ? 4 : 3);

			m_blockersWanted = max(2, m_blockersWanted);
		}
//		if (m_zerglings > 10) m_blockersWanted = min(9, m_blockersWanted + m_zerglings/5);

		if (m_zerglings < 15)
		{
			if (me().CompletedUnits(Terran_Vulture) >= 1)
				m_blockersWanted = 0;

			if (m_zerglings < 10)
				if (m_snipersAvailable)
					if (marinesAroundBlockingPos >= 2)
						if (m_zerglings/3 < marinesAroundBlockingPos)
							m_blockersWanted = 0;
		}
		else
		{
//			if (me().CompletedBuildings(Terran_Bunker) >= 2)
//				m_blockersWanted = max(2, m_blockersWanted - max(0, (marinesAroundBlockingPos-5)));

			if (me().CompletedUnits(Terran_Vulture) >= 2)
				if (me().CompletedBuildings(Terran_Bunker) >= 2)
					m_blockersWanted = 0;

			if (me().CompletedUnits(Terran_Vulture) >= 1)
				if (me().CompletedBuildings(Terran_Bunker) >= 2)
					m_blockersWanted = 1;
		}

///		if (me().CompletedBuildings(Terran_Bunker) >= 2) m_blockersWanted = 0;


		if (m_blockersWanted <= 2)
			if (m_snipersAvailable && (marinesAroundBlockingPos >= 2))
				if (Mining::Instances().size() <= 6)
					m_blockersWanted = max(0, (int)Mining::Instances().size() - 5);

		const int blockers = count_if(Blocking::Instances().begin(), Blocking::Instances().end(), [](const Blocking * pBlocker)
								{ return pBlocker->State() != Blocking::dragOut; });

		if (blockers < m_blockersWanted)
		{
		///	bw << "blockers = " << blockers << " blockersWanted = " << m_blockersWanted << endl;
			if (My<Terran_SCV> * pWorker = findFreeWorker(me().StartingVBase(), [](const My<Terran_SCV> * pSCV)
						{ return pSCV->Life() > 50; }))
				return pWorker->ChangeBehavior<Blocking>(pWorker, ext(pDefenseCP));
		}
		else if (blockers > m_blockersWanted)
		{
		///	bw << "blockers = " << blockers << " blockersWanted = " << m_blockersWanted << endl;
			int minLife = numeric_limits<int>::max();
			MyUnit * pBlockerToRemove = nullptr;
			for (Blocking * pBlocker : Blocking::Instances())
				if (pBlocker->State() != Blocking::dragOut)
					if (pBlocker->Agent()->Life() < minLife)
					{
						minLife = pBlocker->Agent()->Life();
						pBlockerToRemove = pBlocker->Agent();
					}

			if (pBlockerToRemove)
				return pBlockerToRemove->ChangeBehavior<DefaultBehavior>(pBlockerToRemove);
		}
	}
}


void ZerglingRush::WorkerDefense()
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


bool ZerglingRush::TechRestartingCondition() const
{
	if (m_techRestarting) return true;

	int zerglings = him().AllUnits(Zerg_Zergling).size();

	if (!m_snipersAvailable) return false;

//	if (!me().Buildings(Terran_Factory).empty())
//		return m_techRestarting = true;

	if ((m_blockersWanted <= 2) || (me().MineralsAvailable() > 150))
		if ((Mining::Instances().size() >= 9)
			/*(Mining::Instances().size() >= 12) ||
			(Mining::Instances().size() >= 11) && (m_zerglings < 9) ||
			(Mining::Instances().size() >= 10) && (m_zerglings < 6) ||
			(Mining::Instances().size() >= 9) && (m_zerglings < 3)*/)
			if (me().UnitsBeingTrained(Terran_SCV) ||
				(me().SupplyAvailable() == 0) && me().BuildingsBeingTrained(Terran_Supply_Depot))
				if ((Constructing::Instances().size() == 0) ||
					(Constructing::Instances().size() == 1) &&
						(me().BuildingsBeingTrained(Terran_Supply_Depot) || me().BuildingsBeingTrained(Terran_Bunker)) ||
					(Constructing::Instances().size() == 2) &&
						(me().BuildingsBeingTrained(Terran_Supply_Depot) && me().BuildingsBeingTrained(Terran_Bunker)))
				{
					if (me().MineralsAvailable() > 100)
						return m_techRestarting = true;
/*
					if (me().MineralsAvailable() > 150)
						return m_techRestarting = true;

					if (me().MineralsAvailable() > 115)
						if (me().CompletedUnits(Terran_Marine)*2 + me().CompletedUnits(Terran_Vulture)*3 > zerglings)
							return m_techRestarting = true;

					if ((int)me().Units(Terran_Marine).size() >= MaxMarines())
						if (!me().Buildings(Terran_Factory).empty())
							return m_techRestarting = true;
*/
				}

	return false;

/*
	if (!me().Buildings(Terran_Factory).empty())
		if (me().Units(Terran_Vulture).size() < 2)
			return false;

	if (me().Buildings(Terran_Factory).size() < 2)
		if ((me().MineralsAvailable() > 150) && (me().SupplyAvailable() >= 1))
			return true;

	int zerglings = him().AllUnits(Zerg_Zergling).size();

	return me().CompletedUnits(Terran_Marine)*2 + me().CompletedUnits(Terran_Vulture)*3 > zerglings;
*/
}



bool ZerglingRush::FastPool() const 
{
//	return m_zerglingCompletedTime && (m_zerglingCompletedTime < 3100);
	return m_timeZerglingHere && (m_timeZerglingHere < 3800);
}


void ZerglingRush::OnFrame_v()
{
	if (ai()->Frame() == 1)
		if (!(him().IsTerran() || him().IsProtoss()))
			if (ai()->GetStrategy()->Has<Walling>())
				return Discard();

	m_zerglings = him().AllUnits(Zerg_Zergling).size();
	
	if (FastPool())
		if (me().CompletedBuildings(Terran_Bunker))
			if (ai()->Frame() % 300 == 0)
				m_hypotheticZerglings = min(7, min(m_hypotheticZerglings + 1, m_zerglings/2));

	m_zerglings += m_hypotheticZerglings;

	
	if (!m_timeZerglingHere)
	{
		if (m_zerglings)
			DO_ONCE
			{
				m_zerglingCompletedTime = ai()->Frame();
				Position cpDefense = center(ai()->Me().GetVArea()->DefenseCP()
										? ai()->Me().GetVArea()->DefenseCP()->Center()
										: me().GetArea()->Top());
				int length_pool_cpDefense;
				ai()->GetMap().GetPath(him().AllUnits(Zerg_Zergling).front()->LastPosition(), cpDefense, &length_pool_cpDefense);
				frame_t timeToReachMe = frame_t(length_pool_cpDefense / UnitType(Zerg_Zergling).topSpeed());
				m_timeZerglingHere = m_zerglingCompletedTime + timeToReachMe;

			///	bw << "ZerglingRush : found Zergling" << endl;
			///	bw << "zerglingCompletedTime : " << m_zerglingCompletedTime << endl;
			///	bw << "timeToReachMe : " << timeToReachMe << endl;
			///	bw << "timeZerglingHere : " << m_timeZerglingHere << endl;
			}

		if ((him().Buildings(Zerg_Spawning_Pool).size() > 0)/* && (ai()->Frame() < 2500)*/)
			DO_ONCE
			{
				HisBuilding * pool = him().Buildings(Zerg_Spawning_Pool).front();
				frame_t poolCompletedTime = ai()->Frame() + pool->RemainingBuildTime();
				m_zerglingCompletedTime = poolCompletedTime + (pool->Completed() ? 75 : 460 + 50);

				Position cpDefense = center(ai()->Me().GetVArea()->DefenseCP()
										? ai()->Me().GetVArea()->DefenseCP()->Center()
										: me().GetArea()->Top());
				int length_pool_cpDefense;
				ai()->GetMap().GetPath(pool->Pos(), cpDefense, &length_pool_cpDefense);
				frame_t timeToReachMe = frame_t(length_pool_cpDefense / UnitType(Zerg_Zergling).topSpeed());
				m_timeZerglingHere = m_zerglingCompletedTime + timeToReachMe;

			///	bw << "ZerglingRush : found Pool" << endl;
			///	bw << "poolCompletedTime : " << poolCompletedTime << endl;
			///	bw << "zerglingCompletedTime : " << m_zerglingCompletedTime << endl;
			///	bw << "timeToReachMe : " << timeToReachMe << endl;
			///	bw << "timeZerglingHere : " << m_timeZerglingHere << endl;

				if (pool->Completed() && !FastPool()) { m_zerglingCompletedTime = m_timeZerglingHere = 0; return; }
			}
	}

	const int bigZerglingRushThreshold = 9;

	m_maxMarines = me().Units(Terran_Marine).size();
	if ((m_zerglings < 10) && (m_blockersWanted > 0) || (m_blockersWanted > 2))
		++m_maxMarines;

	m_maxMarines = max(2, m_maxMarines);
	if (me().Units(Terran_Marine).empty()) m_maxMarines = 1;

///	bw->drawTextScreen(300, 10, "%cm_maxMarines = %d", Text::Yellow, m_maxMarines);

	if (him().IsTerran() || him().IsProtoss()) return Discard();

	if (m_snipersAvailable)
		if (m_zerglings <= bigZerglingRushThreshold)
		{
			if (me().CompletedUnits(Terran_Vulture) >= 2) return Discard();
			if ((me().CompletedUnits(Terran_Vulture) == 1) && (me().CompletedUnits(Terran_Marine) >= 2)) return Discard();
		}
		else
		{
			if (me().CompletedBuildings(Terran_Bunker) >= 1)
			if (me().CompletedUnits(Terran_Marine) >= 4)
			if (me().CompletedUnits(Terran_Vulture) >= 3)
				return Discard();

			if (me().CompletedBuildings(Terran_Bunker) >= 2)
			if (me().CompletedUnits(Terran_Marine) >= 6)
			if (me().CompletedUnits(Terran_Vulture) >= 2)
				return Discard();

			if (me().CompletedBuildings(Terran_Bunker) >= 2)
			if (me().CompletedUnits(Terran_Marine) >= 8)
			if (me().CompletedUnits(Terran_Vulture) >= 1)
				return Discard();
		}

	if (m_detected)
	{
		//if (!Sniping::Instances().empty()) m_snipersAvailable = true;
		if (!m_snipersAvailable)
		{
			if (me().CompletedBuildings(Terran_Bunker) >= 1)
				if (me().CompletedUnits(Terran_Marine) >= 1)
					m_snipersAvailable = true;

			for (Sniping * pSniper : Sniping::Instances())
				if (pSniper->State() == Sniping::sniping)
					m_snipersAvailable = true;
		}


		DO_ONCE
			if (FastPool())
				if (me().CompletedBuildings(Terran_Supply_Depot) >= 1)
					if (me().Buildings(Terran_Barracks).size() >= 1)
						if (Sniping::Instances().empty())
							if (me().Units(Terran_Marine).size() == 0)
			//					if (me().SupplyAvailable() >= 3)
									if (me().MineralsAvailable() < 50)
										for (const auto & b : me().Buildings(Terran_Supply_Depot))
											if (!b->Completed())
												if (b->CanAcceptCommand())
												{
												///	bw << "cancel Depot a" << endl;
												///	ai()->SetDelay(500);
													return b->CancelConstruction();
												}


		if (TechRestartingCondition())
		{
			DO_ONCE
				ai()->GetStrategy()->SetMinScoutingSCVs(ai()->GetStrategy()->MinScoutingSCVs() + 1);
		}
		else
		{

			if (FastPool())
				for (const auto & b : me().Buildings(Terran_Refinery))
					if (!b->Completed())
						if (b->CanAcceptCommand())
							DO_ONCE
							{
							///	bw << "cancel Refinery" << endl;
							///	ai()->SetDelay(500);
								return b->CancelConstruction();
							}

			DO_ONCE
				if (FastPool())
					for (Constructing * pBuilder : Constructing::Instances())
						if (pBuilder->Type() == Terran_Refinery)
							return pBuilder->Agent()->ChangeBehavior<DefaultBehavior>(pBuilder->Agent());


			if (me().Units(Terran_Vulture).size() == 0)
				if (Mining::Instances().size() < 6)
					if (me().MineralsAvailable() < 30)
						if (!(me().UnitsBeingTrained(Terran_SCV) && me().UnitsBeingTrained(Terran_Marine)))
						{
							MyBuilding * pLatestUncompletedFactory = nullptr;
							for (const auto & b : me().Buildings(Terran_Factory))
								if (!b->Completed())
									if (!pLatestUncompletedFactory || (b->RemainingBuildTime() > pLatestUncompletedFactory->RemainingBuildTime()))
										pLatestUncompletedFactory = b.get();

							if (pLatestUncompletedFactory)
								if (pLatestUncompletedFactory->RemainingBuildTime() > 750)
									if (pLatestUncompletedFactory->CanAcceptCommand())
									{
									///	bw << "cancel Factory" << endl;
									///	ai()->SetDelay(500);
										return pLatestUncompletedFactory->CancelConstruction();
									}
						}

			static frame_t lastCancel = 0;
			if (ai()->Frame() - lastCancel > 3*bw->getRemainingLatencyFrames())
			{
				MyBuilding * pBarrack = me().Buildings(Terran_Barracks).empty() ? nullptr : me().Buildings(Terran_Barracks).front().get();

				if (me().Units(Terran_Vulture).size() == 0)
					if (me().SupplyAvailable() >= 1)
						if (me().MineralsAvailable() + 3*(int)Mining::Instances().size() < 50)
							if (pBarrack && pBarrack->Completed() && pBarrack->CanAcceptCommand() && !pBarrack->Unit()->isTraining())
								if ((int)me().Units(Terran_Marine).size() < 2)
								{
									if (FastPool())
										for (const auto & b : me().Buildings(Terran_Command_Center))
											if (b->Unit()->isTraining())
												if (b->TimeToTrain() > 50)
													if (b->CanAcceptCommand())
													{
													///	bw << "cancel SCV a" << endl;
													///	ai()->SetDelay(500);
														lastCancel = ai()->Frame();
														return b->CancelTrain();
													}

									if (FastPool())
										for (const auto & b : me().Buildings(Terran_Supply_Depot))
											if (!b->Completed())
												if (b->CanAcceptCommand())
												{
												///	bw << "cancel Depot b" << endl;
												///	ai()->SetDelay(500);
													lastCancel = ai()->Frame();
													return b->CancelConstruction();
												}
								}

				if (FastPool())
					if (me().Units(Terran_Vulture).size() == 0)
						if (me().SupplyAvailable() == 0)
							if (pBarrack && pBarrack->Completed() && pBarrack->CanAcceptCommand() && !pBarrack->Unit()->isTraining())
								if ((int)me().Units(Terran_Marine).size() < 2)
									for (const auto & b : me().Buildings(Terran_Command_Center))
										if (b->Unit()->isTraining())
											if (b->CanAcceptCommand())
											{
											///	bw << "cancel SCV b" << endl;
											///	ai()->SetDelay(500);
												lastCancel = ai()->Frame();
												return b->CancelTrain();
											}
			}
		}

//		if (ai()->Frame() >= (ai()->GetMap().StartingLocations().size() == 2 ? 3050 : 2950))
		if (ai()->Frame() >= m_timeZerglingHere - 250)
			ChokePointDefense();

		WorkerDefense();
	}
	else
	{
		if (me().CompletedUnits(Terran_Vulture) >= 1) return Discard();

		if (me().SCVsoldiers() == 1)
		{
			if ((m_zerglings >= 1) && !FastPool())
				ai()->GetStrategy()->SetMinScoutingSCVs(2);
			else
				for (Fleeing * pFleeingSCV : Fleeing::Instances())
					if (ai()->Frame() - pFleeingSCV->Since() > 500)
						ai()->GetStrategy()->SetMinScoutingSCVs(2);
		}
		
		if ((m_zerglings >= 1) || FastPool())
		{
		///	ai()->SetDelay(50);

/*
			bool threat = true;
			for (const auto & b : me().Buildings(Terran_Factory))
				if (b->Life() > 700)
					threat = false;
*/
			bool threat = false;


			if (m_zerglings >= 5) threat = true;
			if (FastPool()) threat = true;

			if (threat)
			{
				if (!FastPool())
					for (const auto & b : me().Buildings(Terran_Factory))
						if (m_timeZerglingHere > ai()->Frame() + b->RemainingBuildTime() - 200)
						{
						///	bw << "time Zergling here = " << m_timeZerglingHere << endl;
						///	bw << "time Factory completed = " << ai()->Frame() + b->RemainingBuildTime() << endl;
						///	bw << m_timeZerglingHere - (ai()->Frame() + b->RemainingBuildTime()) << endl;
						///	bw << "--> discard ZerglingRush" << endl;
							return Discard();
						}

				m_detected = true;
				m_detetedSince = ai()->Frame();
				ai()->GetStrategy()->SetMinScoutingSCVs(0);
				
				if (FastPool())
					for (const auto & u : me().Units(Terran_SCV))
						if (!u->GetStronghold())
						{
							u->EnterStronghold(me().Bases().front()->GetStronghold());
							return u->ChangeBehavior<Supplementing>(u->IsMy<Terran_SCV>());
						}
			}
		}
	}
}


void marineHandlingOnZerglingOrHydraPressure()
{
	if (him().ZerglingPressure()/* || him().HydraPressure()*/)
		if (const ChokePoint * pDefenseCP = ai()->Me().GetVArea()->DefenseCP())
		{
			DO_ONCE
				for (const auto & u : me().Units(Terran_Marine))
					if (u->Completed() && !u->Loaded())
						u->ChangeBehavior<DefaultBehavior>(u.get());

			Position marinesBlockingPos = center(pDefenseCP->Center());
			for (const auto & u : me().Units(Terran_Marine))
				if (u->Completed() && !u->Loaded())
				{
					if (u->GetBehavior()->IsExploring())
					{
						if (dist(u->Pos(), me().StartingBase()->Center()) + 32*3 > dist(center(pDefenseCP->Center()), me().StartingBase()->Center()))
						{
							Position retraitingPos = (me().StartingBase()->Center() + marinesBlockingPos)/2;
							if (ai()->GetMap().GetArea(WalkPosition(retraitingPos)) != me().StartingBase()->GetArea())
								retraitingPos = center(me().StartingBase()->GetArea()->Top());

							u->ChangeBehavior<Raiding>(u.get(), retraitingPos);
						}

						if (dist(u->Pos(), marinesBlockingPos) > 32*6)
						{
							u->ChangeBehavior<Raiding>(u.get(), marinesBlockingPos);
						}
					}
				}
		}
}


} // namespace iron



