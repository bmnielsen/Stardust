//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "kiting.h"
#include "repairing.h"
#include "fleeing.h"
#include "retraiting.h"
#include "vchasing.h"
#include "laying.h"
#include "sieging.h"
#include "dropping1T.h"
#include "dropping1T1V.h"
#include "sniping.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/walling.h"
#include "../strategy/shallowTwo.h"
#include "../strategy/expand.h"
#include "../units/army.h"
#include "../units/factory.h"
#include "../units/starport.h"
#include "../units/bunker.h"
#include "../vect.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{





//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Kiting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Kiting *> Kiting::m_Instances;

Kiting::Kiting(MyUnit * pAgent)
	: Behavior(pAgent, behavior_t::Kiting)
{
	assert_throw(!pAgent->Is(Terran_Siege_Tank_Siege_Mode));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

//	if (pAgent->Is(Terran_Siege_Tank_Tank_Mode))
//		ai()->SetDelay(50);
}


Kiting::~Kiting()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Kiting::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Kiting::StateName() const
{CI(this);
	switch(State())
	{
	case fleeing:			return "fleeing" + (FleeUntil() ? " (" + to_string(FleeUntil() - ai()->Frame()) + ")" : string(""));
	case kiting:			return (GoAwayFromStuckingBuildingUntil() >= ai()->Frame()) ? "goAwayFromStuckingBuilding" : "kiting";
	case patroling:			return "patroling (" + to_string(PatrolTime() - (ai()->Frame() - m_lastPatrol)) + ")";
	case attacking:			return "attacking (" + to_string(AttackTime() - (ai()->Frame() - m_lastAttack)) + ")";
	case cornered:			return "cornered (" + to_string(AttackTime() - (ai()->Frame() - m_lastAttack)) + ")";
	case mining:			return "mining (" + to_string(MinePlacementTime() - (ai()->Frame() - m_lastMinePlacement)) + ")";
	default:				return "?";
	}
}


bool Kiting::ShouldFlee() const
{
	return ai()->Frame() < m_fleeUntil;
}


void Kiting::FleeFor(frame_t time)
{CI(this);
	m_lastPatrol = m_lastAttack = 0;
	m_fleeUntil = ai()->Frame() + time;
///	DO_ONCE ai()->SetDelay(5000);
}


void Kiting::CheckBlockingMarinesAround(int distanceToHisFire) const
{CI(this);
//	if (him().IsProtoss())
	if (ShouldFlee() || (distanceToHisFire < 2*32))
		if (Agent()->Is(Terran_Marine) || Agent()->Is(Terran_Medic))
			if (State() == fleeing)
				for (UnitType type : {Terran_Marine, Terran_Medic})
				for (const auto & u : me().Units(type))
					if (u->Completed() && !u->Loaded())
						if (u.get() != Agent())
						{
							if (u->GetBehavior()->IsHealing())
								u->ChangeBehavior<Kiting>(u.get());

							if (Kiting * pOtherKiter = u->GetBehavior()->IsKiting())
							{
								int d = min(2*32, max(1*32, 6*32-distanceToHisFire*2));

								if (Agent()->Pos().getApproxDistance(u->Pos()) < d)
								{
								///	bw->drawCircleMap(Agent()->Pos(), d, Colors::White);
									pOtherKiter->FleeFor(5);
								}
							}
						}
}


Vect Kiting::CalcultateDivergingVector() const
{CI(this);
	Kiting * pCloser = nullptr;
	const int maxDist = 32*10;
	int minDist = maxDist;

	for (Kiting * pKiting : Instances())
		if (pKiting != this)
			if (pKiting->State() == fleeing)
				if (!pKiting->Agent()->Flying() == !Agent()->Flying())
				{
					int d = Agent()->Pos().getApproxDistance(pKiting->Agent()->Pos());
					if (d < minDist) 
					{
						minDist = d;
						pCloser = pKiting;
					}
				}

	Vect dir;
	if (pCloser)
	{
		dir = toVect(Agent()->Pos() - pCloser->Agent()->Pos());
		dir.Normalize();
		dir *= ((maxDist - minDist) / 2 / (double)maxDist);
	}

	return dir;
}


Vect Kiting::CalcultateHelpVector() const
{CI(this);
	Vect dir;
	if (!(Agent()->Is(Terran_Marine) || Agent()->Is(Terran_Medic))) return dir;

	Kiting * pCloser = nullptr;
	const int maxDist = 32*16;
	int minDist = maxDist;

	for (Kiting * pKiting : Instances())
		if (pKiting != this)
			if ((pKiting->State() != fleeing) && (pKiting->State() != cornered))
				if (pKiting->Agent()->Is(Terran_Marine))
				{
					int d = Agent()->Pos().getApproxDistance(pKiting->Agent()->Pos());
					if (d < minDist) 
					{
						minDist = d;
						pCloser = pKiting;
					}
				}

	if (pCloser)
	{
		Vect delta = toVect(pCloser->Agent()->Pos() - Agent()->Pos());
		double d = delta.Norm();
		dir = delta / d;
		//if (d < 3*32)
			dir /= 2;
	}

	return dir;
}


Vect Kiting::CalcultateThreatVector(const vector<const FaceOff *> & Threats) const
{CI(this);
	Vect dir;
	for (const auto * pFaceOff : Threats)
	{
	//	if (pFaceOff->His()->Is(Zerg_Lurker) && !pFaceOff->His()->Burrowed()) continue;

		Vect d = toVect(Agent()->Pos() - pFaceOff->His()->Pos());
		d.Normalize();
		d *= pFaceOff->HisAttack();

		if (pFaceOff->His()->Type().isWorker())
			for (int i = 1 ; i <= 3 ; ++i)
			{
				TilePosition t((Agent()->Pos()*i + pFaceOff->His()->Pos()*(4-i))/4);
				CHECK_POS(t);
				if (ai()->GetMap().GetTile(t).GetNeutral())
				{
					d /= 5;
					break;
				}
			}

		if (auto * pHisUnit = pFaceOff->His()->IsHisUnit())
			if (!pHisUnit->MinesTargetingThis().empty())
			{
#if DEV
			//	bw->drawTextMap(pHisUnit->Pos().x-5, pHisUnit->Pos().y-8, "%c%d", Text::White, pHisUnit->MinesTargetingThis());
#endif
				
				d /= pFaceOff->HisAttack();
				d *= (pFaceOff->HisAttack() + 50*(int)pHisUnit->MinesTargetingThis().size()) / (1 + (int)pHisUnit->MinesTargetingThis().size());
			}


		if (pFaceOff->His()->CanMove())
		{
			if (pFaceOff->DistanceToHisRange() >= 32) d = d*32/pFaceOff->DistanceToHisRange();
		}
		else
		{
			if (pFaceOff->DistanceToHisRange() >= 4) d = d*4/pFaceOff->DistanceToHisRange();
		}

		dir += d;
#if DEV
		drawLineMap(pFaceOff->His()->Pos(), pFaceOff->His()->Pos() + toPosition(32*d), Colors::White, crop);//1
#endif
	}

	dir.Normalize();
	return dir;
}


Vect Kiting::CalcultateIncreasingAltitudeVector() const
{CI(this);
	assert_throw(!Agent()->Flying());

	const auto w = WalkPosition(Agent()->Pos());
	CHECK_POS(w);
	const altitude_t altitude = ai()->GetMap().GetMiniTile(w).Altitude();
	const altitude_t maxAltitude = Agent()->GetArea()->MaxAltitude();

	Vect dir = toVect(Position(ai()->GetVMap().GetIncreasingAltitudeDirection(WalkPosition(Agent()->Pos()))));
	dir.Normalize();
	dir *= (maxAltitude - min(altitude * 2, (int)maxAltitude)) / 2 / (double)maxAltitude;
	return dir;
}


static map<int, vector<WalkPosition>> DeltasByRadius;


void Kiting::AdjustTarget(Position & target, const int walkRadius) const
{CI(this);
	assert_throw(!Agent()->Flying());

	const int kindOfImpasses =
		((Agent()->GetArea()->AccessibleNeighbours().size() == 1) ||
		(Agent()->GetArea() == him().GetArea()) ||
		 (Agent()->GetArea() == me().GetArea()))
			? 1 : 2;

	vector<Area::id> AreasAllowed = {Agent()->GetArea()->Id()};

//	if (//(State() == anywhere) ||
//		(Agent()->Life()*4 < Agent()->MaxLife()*3) ||
//		(Agent()->Life() < Agent()->PrevLife(30)))
		for (const Area * area : Agent()->GetArea()->AccessibleNeighbours())
			if (!(
					(Agent()->Is(Terran_Vulture) || Agent()->Is(Terran_SCV))
					&& isImpasse(area) && !isImpasse(Agent()->GetArea())
					&& !contains(me().EnlargedAreas(), area)
				))
				AreasAllowed.push_back(area->Id());
#if DEV
			else
				for (const auto & cp : *Agent()->GetArea()->ChokePointsByArea().find(area)->second)
					for (auto p : cp.Geometry())
					{
						bw->drawBoxMap(Position(p), Position(p + 1), Colors::Orange, bool("isSolid"));
						drawLineMap(center(cp.Center()), Agent()->Pos(), Colors::Orange);
					}
#endif

	bool searchLastTarget = (m_lastPos.getApproxDistance(Agent()->Pos()) < 32);
	bool foundLastTarget = false;

	Position bestTarget = target;
	int minDistToTarget = numeric_limits<int>::max();

	vector<WalkPosition> & Deltas = DeltasByRadius[walkRadius];
	if (Deltas.empty())
		for (int dy = -walkRadius ; dy <= +walkRadius ; ++dy)
		for (int dx = -walkRadius ; dx <= +walkRadius ; ++dx)
			if (abs(norm(dx, dy) - walkRadius) < 0.35)
				Deltas.push_back(WalkPosition(dx, dy));

	int candidates = 0;
	for (const WalkPosition & delta : Deltas)
	{
		const WalkPosition w = WalkPosition(Agent()->Pos()) + delta;
		const Position p = center(w);
		if (ai()->GetMap().Valid(w))
		{
			const auto & miniTile = ai()->GetMap().GetMiniTile(w, check_t::no_check);
			if (!contains(AreasAllowed, miniTile.AreaId())) continue;
			const Area * area = ai()->GetMap().GetArea(miniTile.AreaId());
			if (Agent()->GetArea() != area)
				if (all_of(Agent()->GetArea()->ChokePoints(area).begin(), Agent()->GetArea()->ChokePoints(area).end(),
					[p, walkRadius](const ChokePoint & cp){ return ext(&cp)->AirDistanceFrom(p) > walkRadius*8; }))
					continue;
			if (miniTile.Altitude() < 8) continue;

			bool blocked = false;
			for (int iwalkRadius = walkRadius ; iwalkRadius >= 4 ; iwalkRadius -= 4)
			{
				const WalkPosition iDelta = delta * iwalkRadius / walkRadius;
				const WalkPosition iw = WalkPosition(Agent()->Pos()) + iDelta;
//				const auto & iTile = ai()->GetMap().GetTile(TilePosition(iw), check_t::no_check);
				CHECK_POS(TilePosition(iw));
				const auto & iTile = ai()->GetMap().GetTile(TilePosition(iw));

				if ((iTile.MinAltitude() < 32) || (walkRadius == 8))
				{
					if (iTile.GetNeutral() || ai()->GetVMap().GetBuildingOn(iTile) || ai()->GetVMap().Impasse(kindOfImpasses, iTile))
						{ blocked = true; break; }
				}
				else
				{
					if (iTile.GetNeutral() || ai()->GetVMap().GetBuildingOn(iTile) || ai()->GetVMap().Impasse(kindOfImpasses, iTile)
						|| ai()->GetVMap().NearBuildingOrNeutral(iTile))
						{ blocked = true; break; }
				}
				
				const Position ip = center(iw);
				if (any_of(ai()->Critters().Get().begin(), ai()->Critters().Get().end(),
					[ip](const unique_ptr<HisUnit> & critter){ return critter->Pos().getApproxDistance(ip) < 32; }))
					{ blocked = true; break; }
			}
			if (blocked) continue;


			int d = target.getApproxDistance(p);
			if (d < minDistToTarget)
			{
				minDistToTarget = d;
				bestTarget = p;
			}

			if (searchLastTarget && !foundLastTarget)
				if (m_lastTarget.getApproxDistance(p) < 32*3)
					foundLastTarget = true;

#if DEV
		///	drawLineMap(Agent()->Pos(), p, Colors::Black);
#endif
			++candidates;
		}
	}

	if (searchLastTarget && !foundLastTarget && (candidates > 10))
	{
//		markImpasse(TilePosition(m_lastTarget));
	}

	target = bestTarget;
}


int Kiting::SafeDist(const HisBWAPIUnit * pTarget) const
{CI(this);
	const bool immobile = pTarget->IsHisBuilding() != nullptr;
	const bool faster = pTarget->Speed() > Agent()->Speed();
	const bool ranged = pTarget->GroundRange() >= 4*32;

	if (!Agent()->Flying())
		if (const Walling * s = ai()->GetStrategy()->Active<Walling>())
			if (!s->GetWall()->Open())
			{
				if (ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos())))) return 0;
			}

	if (Agent()->Is(Terran_Vulture)) return immobile ? 1*32 : pTarget->Is(Zerg_Hydralisk) ? 3*32+32 : 3*32;
	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode)) return immobile ? 1*32 : faster ? 0*32 : ranged ? 1*32 : 4*32;
	if (Agent()->Is(Terran_Goliath)) return immobile ? 1*32 : pTarget->Is(Zerg_Mutalisk) ? 3*32 : pTarget->Flying() ? 1*32 : 3*32;
	if (Agent()->Is(Terran_Marine)) return immobile ? 1*32 : 60;//55;
	if (Agent()->Is(Terran_Wraith)) return immobile ? 1*32 : 2*32;
	if (Agent()->Is(Terran_Valkyrie)) return immobile ? 1*32 : 2*32;
	if (Agent()->Is(Terran_Dropship)) return immobile ? 1*32 : 2*32;

	return immobile ? 2*32 : 3*32;
}


frame_t Kiting::PatrolTime() const
{CI(this);
	if (Agent()->Is(Terran_Vulture)) return 5;
	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode)) return 16;
	if (Agent()->Is(Terran_Goliath)) return 16;
	if (Agent()->Is(Terran_Marine)) return ai()->GetStrategy()->Active<Walling>() ? 28 : 0;//16;
	if (Agent()->Is(Terran_Wraith)) return 16;
	if (Agent()->Is(Terran_Valkyrie)) return 0;
	if (Agent()->Is(Terran_Dropship)) return 0;

	return 16;
}


frame_t Kiting::AttackTime() const
{CI(this);
	return PatrolTime();
}


frame_t Kiting::MinePlacementTime() const
{CI(this);
	return 14;
}


void Kiting::OnFrame_v()
{CI(this);
	#if DEV
//		if (Agent()->IsMy<Terran_Goliath>()) bw->drawCircleMap(Agent()->Pos(), Agent()->AirRange(), Colors::White);
	#endif

	if (!Agent()->CanAcceptCommand()) return;

	switch (State())
	{
	case fleeing:
	case kiting:
	case patroling:
	case attacking:
	case cornered:
	case mining:
						OnFrame_kiting(); break;
	default: assert_throw(false);
	}
}


static const FaceOff * selectTarget(const vector<const FaceOff *> & Candidates)
{
	assert_throw(!Candidates.empty());

	const FaceOff * pTarget = nullptr;
	int bestScore = numeric_limits<int>::max();

	for (const FaceOff * fo : Candidates)
	{
		int score = fo->DistanceToMyRange() > 1*32
					? 1000000000 + fo->DistanceToMyRange()
					: 10000 * (fo->FramesToKillHim() + max(0, fo->DistanceToMyRange()));

		if (fo->Mine()->Is(Terran_Wraith))
		{
			if (fo->His()->Flying())
				score /= 10;
			if (fo->His()->Is(Zerg_Lurker))
				score /= 10;
			if (fo->His()->Is(Protoss_Dark_Templar))
				score /= 100;
			if (fo->His()->Is(Protoss_Observer))
				score /= 10;
			if (fo->His()->Is(Protoss_High_Templar))
				score /= 100;
			if (fo->His()->Is(Protoss_Dark_Archon))
				score /= 100;
			if (fo->His()->Is(Protoss_Reaver))
				score /= 100;
			if (fo->His()->Is(Protoss_Zealot) ||
				fo->His()->Is(Protoss_Dragoon) ||
				fo->His()->Is(Zerg_Hydralisk) ||
				fo->His()->Is(Terran_Goliath))
				continue;

			switch(fo->His()->Type())
			{
			case Protoss_Shuttle:			score = 1;	break;
			case Protoss_Dark_Templar:		score = 2;	break;
			case Protoss_Observer:			score = 3;	break;
			case Protoss_High_Templar:		score = 4;	break;
			case Protoss_Dark_Archon:		score = 5;	break;
			case Protoss_Reaver:			score = 6;	break;
			case Protoss_Arbiter:			if (fo->Mine()->Unit()->isCloaked()) score = 3;	break;

			case Terran_Dropship:			score = 1;	break;
			case Terran_Science_Vessel:		score = 2;	break;

			case Zerg_Lurker:				score = 0;	break;
			case Zerg_Overlord:				score = 1;	break;
			case Zerg_Queen:				score = 2;	break;
			case Zerg_Defiler:				score = 3;	break;
			}
		}

		if (fo->Mine()->Is(Terran_Valkyrie))
		{
			if (fo->His()->Is(Protoss_Interceptor))
				score /= 100;
		}

	//	if (fo->His()->Is(Protoss_Carrier))
	//		score /= 100;

		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (fo->Mine()->Is(Terran_Marine))
				if (fo->His()->Is(Zerg_Zergling))
					if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(fo->Mine()->Pos()))))
					if (!contains(me().EnlargedAreas(), fo->Mine()->GetArea()))
					if (s->GetWall()->DistanceTo(fo->His()->Pos()) > 8*32)
					{
					///	bw << fo->Mine()->NameWithId() << " DO NOT ATTACK !!!" << endl;
						continue;
					}

		if (score < bestScore)
		{
			bestScore = score;
			pTarget = fo;
		}

	}

//	if (pTarget->Mine()->Flying())
///	if (Candidates.size() >= 2)
///	{
///		bw << "Selected " << pTarget->His()->NameWithId() << " (" << pTarget->His()->LifeWithShields() << ") out of " << endl;
///
///		for (const FaceOff * fo : Candidates)
///			bw << fo->His()->NameWithId() << " (" << fo->His()->LifeWithShields() << ")" << endl;
///		bw << endl;
///		DO_ONCE ai()->SetDelay(500);
///	}

	return pTarget;
}


bool Kiting::VultureFeast() const
{CI(this);
	return ai()->Frame() - m_lastVultureFeast < 60;
}


bool Kiting::TestVultureFeast(const vector<const FaceOff *> & Threats5) const
{
	if (none_of(him().Buildings().begin(), him().Buildings().end(), [this](const unique_ptr<HisBuilding> & b)
			{ return !b->Flying() && b->GetArea() == Agent()->GetArea() && b->Type().isResourceContainer(); }))
		return false;

	int hisWorkers = 0;
	int hisMutas = 0;
	int hisOthers = 0;
	for (const FaceOff * fo : Threats5)
	{
		switch (fo->His()->Type())
		{
		case Terran_SCV:
		case Protoss_Probe:
		case Zerg_Drone:
									++hisWorkers; break;
		case Zerg_Mutalisk:			++hisMutas; break;
		default:					++hisOthers; break;
		}
	}

	if (!hisWorkers) return false;

	if (hisOthers) return false;

	if (hisMutas)
	{
		int vulturesHere = 0;
		for (const auto & u : me().Units(Terran_Vulture))
			if (roundedDist(Agent()->Pos(), u->Pos()) < 10*32)

		if (hisMutas > vulturesHere) return false;
	}

	if (Agent()->Life() < Agent()->MaxLife()/2)
		if (Agent()->Life() < Agent()->PrevLife(10))
			if (!hisMutas && !hisOthers)
				return false;

	return true;
}


void Kiting::OnFrame_kiting()
{CI(this);
	int safeDistanceMod = 0;
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		m_lastPos = Positions::None;
		m_lastPatrol = 0;
		m_lastAttack = 0;
		m_lastMinePlacement = 0;
		m_lastTimeBlockedByMines = 0;

		safeDistanceMod = -32;

///		bw << Agent()->NameWithId() << "chases " << Target()->NameWithId() << endl; ai()->SetDelay(5000);
	}


	if (GoAwayFromStuckingBuildingUntil() >= ai()->Frame())
		return;
	else
	//	if (Agent()->GetArea() != me().GetArea())
		if (!Agent()->Flying())
			if (ai()->Frame() - m_inStateSince > 15)
				if (CheckStuckBuilding())
					return;

	if (!Agent()->Is(Terran_Marine) || ai()->GetStrategy()->Detected<ZerglingRush>() || him().ZerglingPressure())
		if (HisUnit * pTarget = findVChasingAloneTarget(Agent()->IsMyUnit()))
			return Agent()->ChangeBehavior<VChasing>(Agent()->IsMyUnit(), pTarget);
/* TODO g
	if (Agent()->Life() > 30)
		if (HisUnit * pTarget = findVChasingHelpSCVs(Agent()->IsMyUnit()))
			if (dist(pTarget->Pos(), Agent()->Pos()) > 10*32)
				return Agent()->ChangeBehavior<VChasing>(Agent()->IsMyUnit(), pTarget);
*/


	if (Sieging::EnterCondition(Agent()))
		return Agent()->ChangeBehavior<Sieging>(Agent()->As<Terran_Siege_Tank_Tank_Mode>());
	if (Dropping1T1V::EnterCondition(Agent()))
		return Agent()->ChangeBehavior<Dropping1T1V>(Agent()->As<Terran_Dropship>());

	vector<const FaceOff *> Threats5;
	int distanceToHisFire;
	const bool kiteCondition = Kiting::KiteCondition(Agent()->IsMyUnit(), &Threats5, &distanceToHisFire);

	bool marineVsInterceptor = false;
	if (Agent()->Is(Terran_Marine))
	{
		if (him().IsProtoss())
			if (any_of(Threats5.begin(), Threats5.end(), [](const FaceOff * fo){ return fo->His()->Is(Protoss_Interceptor); }))
				marineVsInterceptor = true;

		bool keepAwayFromBlockers =
			ai()->GetStrategy()->Detected<ZerglingRush>() &&
			ai()->Me().GetVArea()->DefenseCP()->Center() &&
			(dist(Agent()->Pos(), center(ai()->Me().GetVArea()->DefenseCP()->Center())) < 2*32);

		if (kiteCondition || keepAwayFromBlockers)
		{
			if (My<Terran_Bunker> * pBunker = Sniping::FindBunker(Agent(), 6*32))
				return Agent()->ChangeBehavior<Sniping>(Agent(), pBunker);

			if (Agent()->NotFiringFor() < 20)
				if (My<Terran_Bunker> * pBunker = Sniping::FindBunker(Agent(), 25*32))
					return Agent()->ChangeBehavior<Sniping>(Agent(), pBunker);

			if (me().HasResearched(TechTypes::Stim_Packs))
				if (Agent()->Unit()->canUseTechWithoutTarget(TechTypes::Stim_Packs))
					if (Agent()->Life() >= 31)
						if (!Agent()->Unit()->isStimmed())
							if (any_of(Threats5.begin(), Threats5.end(), [](const FaceOff * fo){ return fo->His()->IsHisUnit(); }))
							{
/*
								bool noNeedToStim = false;
								if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
									if (pShallowTwo->KeepMarinesAtHome())
										if (contains(me().EnlargedAreas(), Agent()->GetArea(check_t::no_check)))
											if (none_of(Threats5.begin(), Threats5.end(), [](const FaceOff * fo)
												{ return fo->His()->IsHisUnit() &&
													!fo->His()->Flying() &&
													contains(me().EnlargedAreas(), fo->His()->GetArea(check_t::no_check)); }))
												noNeedToStim = true;

								if (!noNeedToStim)
*/
									return Agent()->StimPack();
							}
/*
			if (ai()->GetStrategy()->Detected<ZerglingRush>())
				if (HisUnit * pTarget = findVChasingAloneTarget(Agent()->IsMyUnit()))
					return Agent()->ChangeBehavior<VChasing>(Agent()->IsMyUnit(), pTarget);
*/
		}
	}

	bool enemyTargetedByMines = false;
	bool reaverHere = false;
	if (!Agent()->Flying())
		for (const auto * pFaceOff : Threats5)
			if (auto * pHisUnit = pFaceOff->His()->IsHisUnit())
			{
				if (pHisUnit->Is(Protoss_Reaver) || pHisUnit->Is(Protoss_Scarab)) reaverHere = true;

				if (!pHisUnit->MinesTargetingThis().empty() && (pFaceOff->GroundDistanceToHitHim() < 6*32))
					enemyTargetedByMines = true;
			}

	if (!enemyTargetedByMines && !reaverHere)
	{
		if (ai()->Frame() - m_lastPatrol < PatrolTime()) return;
		if (ai()->Frame() - m_lastAttack < AttackTime()) return;
		if (ai()->Frame() - m_lastMinePlacement < MinePlacementTime()) return;
	}
	m_state = kiting;

#if DEV
	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
	{
//		bw->drawCircleMap(Agent()->Pos(), Agent()->GroundRange(), Colors::Green);
//		for (const auto & faceOff : Agent()->FaceOffs())
//			if (faceOff.DistanceToMyRange() <= 0)
//				drawLineMap(faceOff.His()->Pos(), Agent()->Pos(), Colors::Green);
//
//		bw->drawCircleMap(Agent()->Pos(), 5*32, Colors::Red);
//		for (const auto * pFaceOff : Threats5)
//			drawLineMap(pFaceOff->His()->Pos(), Agent()->Pos(), Colors::Red);
	}
#endif

	if (kiteCondition)
	{
		if (!ShouldFlee())
		{
			if ((Agent()->Is(Terran_Vulture) ||
				Agent()->Is(Terran_Marine) &&
					((Agent()->GetArea(check_t::no_check) == him().GetArea()) ||
					(Agent()->GetArea(check_t::no_check) == him().NaturalArea()))
				) &&
				TestVultureFeast(Threats5))
				m_lastVultureFeast = ai()->Frame();

			if (!enemyTargetedByMines && !reaverHere)
				if (!Agent()->CoolDown())
				if (!Agent()->Flying() || Agent()->Unit()->isMoving())
					if (Agent()->Unit()->isMoving() || (m_lastVultureFeast == ai()->Frame()))
						if ((Agent()->Life()*3 > Agent()->MaxLife()) ||
							Agent()->StayInArea() ||
							!Agent()->WorthBeingRepaired())
						{
							if (!((Agent()->Life()*3 > Agent()->MaxLife()) || Agent()->StayInArea() || !Agent()->WorthBeingRepaired()))
							{
							///	bw << Agent()->NameWithId() << " TestVultureFeast!" << endl;
							///	ai()->SetDelay(500);
							}

							bool invicibleEnemy = false;
							vector<const FaceOff *> DangerousThreats;
							vector<const FaceOff *> Targets;
							for (const auto * pFaceOff : Threats5)
							{
								if (pFaceOff->DistanceToHisRange() < SafeDist(pFaceOff->His()) + safeDistanceMod)
									if (!(Agent()->Is(Terran_Marine) && pFaceOff->His()->Is(Protoss_Interceptor)))
										DangerousThreats.push_back(pFaceOff);

								if (pFaceOff->MyAttack())
									if (pFaceOff->DistanceToMyRange() < 1*32)
										if (!pFaceOff->His()->InFog())
											Targets.push_back(pFaceOff);
							}

							bool badGroundPosition = false;
							//const bool onHigherGround = ai()->GetMap().GetTile(TilePosition(Agent()->Pos())).HigherGround();

							int dangerous_Hydralisks = 0;
							int dangerous_Marines = 0;
							int dangerous_evenBehind = 0;
							int dangerous_butBehind = 0;
							bool behind = true;


							for (const auto * pFaceOff : DangerousThreats)
							{

	/*
								if (!onHigherGround)
									if (ai()->GetMap().GetTile(TilePosition(pFaceOff->His()->Pos())).HigherGround())
									{
										bw << "badGroundPosition : " << Agent()->NameWithId() << endl;
										ai()->SetDelay(200);
										badGroundPosition = true;
										break;
									}
	*/
								if ((pFaceOff->HisRange() < 4*32) ||
									!pFaceOff->MyAttack() ||
									pFaceOff->His()->Is(Terran_Goliath) ||
									pFaceOff->His()->Is(Terran_Siege_Tank_Tank_Mode) ||
									pFaceOff->His()->Is(Terran_Siege_Tank_Siege_Mode) ||
									pFaceOff->His()->Is(Terran_Bunker) ||
								//	pFaceOff->His()->Is(Terran_Marine) && Agent()->Flying() ||
									pFaceOff->His()->Is(Terran_Missile_Turret) ||
									pFaceOff->His()->Is(Protoss_Photon_Cannon) ||
									pFaceOff->His()->Is(Protoss_Dragoon) ||
									pFaceOff->His()->Is(Protoss_Reaver) ||
									pFaceOff->His()->Is(Protoss_Scarab) ||
									pFaceOff->His()->Is(Zerg_Sunken_Colony))
								{
									if (!pFaceOff->MyAttack())
									{
										invicibleEnemy = true;
										break;
									}
	/*
									if (pFaceOff->His()->Is(Zerg_Lurker) && !pFaceOff->His()->InFog() && (ai()->Frame() - pFaceOff->His()->LastFrameMoving() > 10))
									{
										ai()->SetDelay(500);
										bw->drawTriangleMap(pFaceOff->His()->Pos() + Position(0, -15), pFaceOff->His()->Pos() + Position(-15, 0), pFaceOff->His()->Pos() + Position(+15, 0), Colors::White);
										invicibleEnemy = true;
										break;
									}
	*/
									if (pFaceOff->His()->Is(Terran_Goliath) && !Agent()->Flying() ||
										pFaceOff->His()->Is(Terran_Siege_Tank_Tank_Mode) ||
										pFaceOff->His()->Is(Protoss_Dragoon) && !Agent()->Flying())
										++dangerous_butBehind;
									else
									{
										++dangerous_evenBehind;
										break;
									}
								}

								if (pFaceOff->His()->Is(Zerg_Hydralisk)) ++dangerous_Hydralisks;
								if (pFaceOff->His()->Is(Terran_Marine)) ++dangerous_Marines;

								Vect AT = toVect(pFaceOff->His()->Pos() - Agent()->Pos());
								Vect Aacc(Agent()->Acceleration());
								Vect Tacc(pFaceOff->His()->Acceleration());
								if ((Aacc.Norm() > 0.001) && (Tacc.Norm() > 0.001))
								{
									AT.Normalize();
									Aacc.Normalize();
									Tacc.Normalize();
									if ((Aacc * Tacc < 0.1) || (AT * Tacc < 0.1))
									{
										//bw << Agent()->Unit()->getID() << "  " << Aacc.x << "," << Aacc.y << "  " << Tacc.x << "," << Tacc.y << "  " << AT.x << "," << AT.y << endl;
										behind = false;
									}
								}
								else behind = false;
								if (!behind && dangerous_butBehind) break;
							}

							bool flee = invicibleEnemy ||
											dangerous_evenBehind ||
											(!behind && dangerous_butBehind) ||
											(!behind && dangerous_Hydralisks >= 2) ||
											(!behind && dangerous_Marines >= 4) ||
											(!behind && dangerous_Marines >= 2 && Agent()->Is(Terran_Wraith)) ||
											(dangerous_Hydralisks >= 1 && Agent()->Is(Terran_Wraith)) ||
											badGroundPosition;

							if (!flee)
								if (him().IsProtoss())
									if (him().HasBuildingsInNatural())
										if (!Agent()->Flying())
											if (!contains(ext(him().GetArea())->EnlargedArea(), Agent()->GetArea()))
											{
												for (auto fo : Threats5)
													if (!(fo->His()->Is(Protoss_Carrier) || fo->His()->Is(Protoss_Interceptor)))
														if (fo->His()->Flying() ||
															!contains(ext(him().GetArea())->EnlargedArea(), fo->His()->GetArea()))
															flee = true;
											}


			#if DEV
						///	bw->drawCircleMap(Agent()->Pos(), 25, !flee ? Colors::Red : Colors::White);
			#endif

							if (Agent()->Is(Terran_Marine))
								if (him().IsZerg())
									if (const Walling * s = ai()->GetStrategy()->Active<Walling>())
										if (!s->GetWall()->Open())
											if (ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))
												if ((s->GetWall()->DistanceTo(Agent()->Pos()) > 9*32) ||
													!all_of(Targets.begin(), Targets.end(), [](const FaceOff * fo){ return fo->His()->Type().isWorker(); }))
												flee = true;

							if (auto * pWraith = Agent()->IsMy<Terran_Wraith>())
								if ((pWraith->Energy() >= 120) ||
									!flee && (pWraith->Energy() >= 60) ||
									!flee && (pWraith->Energy() >= 40) && (pWraith->Life() < 90) ||
									flee && (pWraith->Energy() >= 25) && (pWraith->Life() < 60))
										if (!pWraith->Unit()->isCloaked())
											if (me().HasResearched(TechTypes::Cloaking_Field))
												if (Agent()->Unit()->canUseTechWithoutTarget(TechTypes::Cloaking_Field))
													if (pWraith->Life() < pWraith->PrevLife(5))
														return pWraith->Cloak();
							if (!flee)
							{
								if (auto * pVessel = Agent()->IsMy<Terran_Science_Vessel>())
									if (pVessel->Energy() >= Cost(TechTypes::Defensive_Matrix).Energy())
									{
										for (UnitType type : {Terran_Siege_Tank_Tank_Mode, Terran_Wraith, Terran_Vulture, Terran_Goliath})
										for (auto & u : me().Units(type))
											if (u->Completed() && !u->Loaded())
												if (u->Life() < u->PrevLife(5))
													if (u->GetBehavior()->IsKiting() ||
														u->GetBehavior()->IsFighting() ||
														u->GetBehavior()->IsVChasing())
															if (roundedDist(u->Pos(), pVessel->Pos()) < 10*32)
																return pVessel->DefensiveMatrix(u.get());
									}

								if (!Targets.empty())
								{
									const FaceOff * pTarget = selectTarget(Targets);
									if (!pTarget) return;

									TilePosition middle((Agent()->Pos() + pTarget->His()->Pos()) / 2);
									if (Agent()->Is(Terran_Marine) ||
										Agent()->Is(Terran_Valkyrie) ||
										Agent()->Is(Terran_Wraith) && pTarget->His()->Flying() ||
										//Agent()->Is(Terran_Wraith) && Agent()->Unit()->isCloaked() ||
										!pTarget->His()->Flying() && !Agent()->Flying() && ai()->GetVMap().EnemyBuildingsAround(middle, 3) ||
										!Agent()->Flying() && ai()->GetStrategy()->Active<Walling>() && (ai()->GetStrategy()->Active<Walling>()->GetWall()->DistanceTo(Agent()->Pos()) < 10*32))
									{
										m_lastAttack = ai()->Frame();
										m_state = attacking;
										Agent()->Attack(pTarget->His());
									///	bw << "111" << endl;
										return;
									}

									Vect AT = toVect(pTarget->His()->Pos() - Agent()->Pos());
									Vect patrolDir1 = rot(AT, pi/5);
									Vect patrolDir2 = rot(AT, -pi/5);
									Vect patrolDir = (patrolDir1 * Agent()->Acceleration() >= 0) ? patrolDir1 : patrolDir2;
									patrolDir.Normalize();
									Position patrolPos = ai()->GetMap().Crop(Agent()->Pos() + toPosition(patrolDir * (Agent()->GroundRange()-5)));
									assert_throw_plus(patrolPos.x >= 10 || patrolPos.y >= 10, "patrol " + my_to_string(patrolPos));

								//	Position patrolPos = ai()->GetVMap().RandomPosition(pBestFaceOff->His()->Pos(), 64);


			#if DEV
								///	for (int i = 3 ; i <= 9 ; i += 3) bw->drawCircleMap(patrolPos, i, GetColor());
								///	drawLineMap(patrolPos, Agent()->Pos(), GetColor());
								///	drawLineMap(patrolPos, pTarget->His()->Pos(), GetColor());
			#endif

									m_lastPatrol = ai()->Frame();
									m_state = patroling;
									return Agent()->Patrol(patrolPos);
								}

								return;
							}
						}

			if (!Agent()->Unit()->isIdle())
				if (all_of(Threats5.begin(), Threats5.end(), [this](const FaceOff * fo)
							{ return fo->DistanceToHisRange() >= SafeDist(fo->His()); }))
					return;

			if (Threats5.size() == 1)
				if (Threats5.front()->His()->Is(Terran_Vulture_Spider_Mine))
					if (Threats5.front()->His()->InFog())
					{
						m_lastTimeBlockedByMines = ai()->Frame();
	#if DEV
					///	bw << Agent()->NameWithId() << " blocked by mines!!" << endl;
					///	for (int x = -2 ; x <= +2 ; ++x)
					///	for (int y = -2 ; y <= +2 ; ++y)
					///		drawLineMap(Agent()->Pos() + Position(x, y), Threats5.front()->His()->Pos() + Position(x, y), Colors::Red);
					///	ai()->SetDelay(50);
	#endif
					}

			if ((Agent()->Pos() == Agent()->PrevPos(5)) && (Agent()->Pos() == Agent()->PrevPos(10)))
				if (!Agent()->CoolDown())
					if (Agent()->Life() < Agent()->PrevLife(50))
					{
						vector<const FaceOff *> Targets;
						for (const auto * pFaceOff : Threats5)
						{
							if (pFaceOff->MyAttack())
								if (pFaceOff->DistanceToMyRange() < 0)
									if (!pFaceOff->His()->InFog())
										Targets.push_back(pFaceOff);
						}
						if (!Targets.empty())
						{
							if (const FaceOff * pTarget = selectTarget(Targets))
							{
								m_lastAttack = ai()->Frame();
								m_state = cornered;
								Agent()->Attack(pTarget->His());
							///	bw << "222" << endl;
		#if DEV
							///	bw << Agent()->NameWithId() << " cornered!!" << endl;
							///	for (int i = 0 ; i < 5 ; ++i)
							///		bw->drawBoxMap(Agent()->Pos() - 15 - i, Agent()->Pos() + 15 + i, Colors::Blue);
							///	ai()->SetDelay(500);
		#endif
							}
							return;
						}
					}
			}

		if (m_lastVultureFeast == ai()->Frame()) return;

		m_state = fleeing;

		if (!marineVsInterceptor) CheckBlockingMarinesAround(distanceToHisFire);

		Vect threatForce = CalcultateThreatVector(Threats5);
		Vect divergingForce = CalcultateDivergingVector();
		Vect helpForce = CalcultateHelpVector();
		Vect altitudeForce;
		Vect dir = threatForce + divergingForce;
		if (!Agent()->Flying())
		{
			double altitudeCoef = 3 * min(4*32, max(0, distanceToHisFire)) / double(4*32);
			if (Agent()->Is(Terran_Vulture)) altitudeCoef *= 2;
			altitudeForce = altitudeCoef * CalcultateIncreasingAltitudeVector();
			dir += altitudeForce;
		}

		Vect normedDir = dir;
		normedDir.Normalize();
		if (double h = helpForce.Norm())
			if (normedDir * (helpForce/h) < 0)
			{
				dir += helpForce;
				normedDir = dir;
				normedDir.Normalize();
			}

		const int buildingsAndNeutralsAround = ai()->GetVMap().BuildingsAndNeutralsAround(TilePosition(Agent()->Pos()), 3);
		const int walkRadius =	(buildingsAndNeutralsAround == 0 || Agent()->Flying()) ? 24 :
								(buildingsAndNeutralsAround == 1) ? 12 : 8;

		Position delta = toPosition(normedDir * (8*walkRadius));
		Position target = Agent()->Pos() + delta;
		if (Agent()->Flying()) target = ai()->GetMap().Crop(target);
		else				   AdjustTarget(target, walkRadius);
/*
		if (!Agent()->Flying())
			if (Agent()->GetArea(check_t::no_check) == findNatural(me().StartingVBase())->BWEMPart()->GetArea())
				if (!Threats5.front()->His()->Flying())
					if (dist(Agent()->Pos(), me().Bases()[0]->BWEMPart()->Center()) <
						dist(Threats5.front()->His()->Pos(), me().Bases()[0]->BWEMPart()->Center()))
					{
					///	bw << Agent()->NameWithId() << " go home" << endl;
						target = me().Bases()[0]->BWEMPart()->Center();
					}
*/
		if (Agent()->Is(Terran_Marine))
			if (auto s = ai()->GetStrategy()->Active<Walling>())
				if (him().IsProtoss())
					if (dist(Agent()->Pos(), s->GetWall()->Center()) < 8*32)
					{
					///	bw << Agent()->NameWithId() << " go home" << endl;
						target = me().GetBase(0)->Center();
					}


		if (ai()->GetMap().Valid(target))
		{
			m_lastPos = Agent()->Pos();
			m_lastTarget = target;
#if DEV
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(threatForce*8*walkRadius), Colors::Red, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(divergingForce*8*walkRadius), Colors::Blue, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(helpForce*8*walkRadius), Colors::Cyan, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(altitudeForce*8*walkRadius), Colors::Yellow, crop);//1
			drawLineMap(Agent()->Pos(), target, GetColor());//1
#endif
			if (Agent()->Is(Terran_Vulture))
				if (!Agent()->GetStronghold())
				{
					if (int numberNeeded = Retraiting::Condition(Agent(), Threats5))
					{
					///	bw << Agent()->NameWithId() << " start Retraiting" << endl;
					///	ai()->SetDelay(500);
						bw->drawTriangleMap(Agent()->Pos() + Position(0, -15), Agent()->Pos() + Position(-10, 0), Agent()->Pos() + Position(+10, 0), Colors::White);
						return Agent()->ChangeBehavior<Retraiting>(Agent(), me().StartingBase()->Center(), numberNeeded);
					}
				}

			return Agent()->Move(target);
		}
	}
	else
	{
		if (Agent()->Unit()->isPatrolling())
			return Agent()->Stop();

		if (me().HasResearched(TechTypes::Spider_Mines))
			if (auto * pVulture = Agent()->IsMy<Terran_Vulture>())
				if (pVulture->RemainingMines() >= (him().IsTerran() ? 2 : 1))
				{
					TilePosition t(pVulture->Pos());
					t.x -= t.x % 3;
					t.y -= t.y % 3;
					CHECK_POS(t);
					if (!ai()->GetVMap().IsThereMine(ai()->GetMap().GetTile(t)))
						if (!contains(me().EnlargedAreas(), Agent()->GetArea()) || !him().AllUnits(Protoss_Dark_Templar).empty())
						{

							const FaceOff * pTargetToMine = nullptr;
							bool outOfHisSight = true;
							for (const auto & faceOff : Agent()->IsMyUnit()->FaceOffs())
							{
	//							if (faceOff.AirDistance() < faceOff.His()->Sight() + 2*32)
								if (faceOff.AirDistanceToHitMe() < faceOff.His()->Sight() + 2*32)
								{
									outOfHisSight = false;
									break;
								}

								if ((faceOff.His()->Is(Terran_Ghost) && !faceOff.His()->Unit()->isDetected()) ||
									faceOff.His()->Is(Terran_Goliath) ||
									faceOff.His()->Is(Terran_Marine) ||
									faceOff.His()->Is(Terran_Siege_Tank_Tank_Mode) ||
									faceOff.His()->Is(Terran_Siege_Tank_Siege_Mode) ||
									faceOff.His()->Is(Protoss_Dragoon) ||
									faceOff.His()->Is(Protoss_Reaver) ||
									(faceOff.His()->Is(Protoss_Dark_Templar) && !faceOff.His()->Unit()->isDetected())  ||
									faceOff.His()->Is(Zerg_Hydralisk) ||
									faceOff.His()->Is(Zerg_Lurker))
									pTargetToMine = &faceOff;
							}

							if (outOfHisSight && pTargetToMine)
							{
								//ai()->SetDelay(500);
								m_lastMinePlacement = ai()->Frame();
								m_state = mining;
								return pVulture->PlaceMine(pVulture->Pos());
							}
						}
				}


		if (VultureFeast())
		{
		///	bw << ai()->Frame() - m_lastVultureFeast << endl;
		}
		else
			if (Agent()->Life() < Agent()->MaxLife())
				if (Agent()->Type().isMechanical())
					if (Agent()->RepairersCount() < Agent()->MaxRepairers())
						if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
							Agent()->StayInArea() ? 8*32 :
							!Agent()->Flying() && (Agent()->GetArea(check_t::no_check) == him().GetArea()) ? 8*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 10*32 :
							Agent()->Flying() && Agent()->IsMyUnit() ? 1000000 :
							(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 30*32 : 1000000))
						{
							return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);
						}

		if (HisUnit * pTarget = Kiting::AttackCondition(Agent(), VultureFeast()))
		{
			if (!Agent()->Flying())
				if (const Walling * s = ai()->GetStrategy()->Active<Walling>())
					if (!s->GetWall()->Open())
					{
						if (ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))
						{
							int minDist = numeric_limits<int>::max();
							MyBuilding * pBuildingTarget = nullptr;
							for (MyBuilding * b : me().Buildings())
								if (b->GetWall() == s->GetWall())
									if (dist(b->Pos(), pTarget->Pos()) < minDist)
									{
										minDist = roundedDist(b->Pos(), pTarget->Pos());
										pBuildingTarget = b;
									}

							vector<TilePosition> Border = outerBorder(pBuildingTarget->TopLeft(), pBuildingTarget->Size());

							minDist = numeric_limits<int>::max();
							TilePosition tileTarget;
							for (TilePosition t : Border) if (ai()->GetMap().Valid(t))
							{
								const Tile & tile = ai()->GetMap().GetTile(t);
								if (ai()->GetVMap().GetBuildingOn(tile)) continue;
								if (tile.GetNeutral()) continue;
								if (!tile.Walkable()) continue;

								if (dist(center(t), Agent()->Pos()) < minDist)
								{
									minDist = roundedDist(center(t), Agent()->Pos());
									tileTarget = t;
								}
							}

							m_lastPatrol = ai()->Frame();
							m_state = patroling;
							Agent()->Patrol(center(tileTarget));
						//	bw->drawCircleMap(center(tileTarget), 5, Colors::White, true);
						//	bw << "!!!" << endl;
							return;
						}
					}

		//	bw << Agent()->NameWithId() << " attack! " << pTarget->NameWithId() << endl;
			m_lastAttack = ai()->Frame();
			m_state = attacking;
			Agent()->Attack(pTarget);

		///	bw << "333" << endl;
			return;
		}


		if (Agent()->Unit()->isIdle())
		{
//			if (HisBuilding * pTarget = findBuildingToRaze(Agent()))
//				return Agent()->Attack(pTarget, check_t::no_check);

			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
		else if (Agent()->Unit()->isPatrolling())
		{
			return Agent()->Stop();
		}
		else
		{
			Agent()->Stop();
		}
	}
}



bool Kiting::KiteCondition(const MyUnit * u, vector<const FaceOff *> * pThreats, int * pDistanceToHisFire)
{
	auto Threats5 = findThreats(u, 5*32, pDistanceToHisFire);
	if (pThreats) *pThreats = Threats5;
	if (Threats5.empty()) return false;
	return true;
}



HisUnit * Kiting::AttackCondition(const MyUnit * u, bool evenWeak)
{
	vector<const FaceOff *> Targets;
	if ((u->Life()*3 > u->MaxLife()) || u->StayInArea() || !u->WorthBeingRepaired() || evenWeak)
		for (const auto & faceOff : u->IsMyUnit()->FaceOffs())
			if (!faceOff.His()->InFog())
				if (faceOff.MyAttack())
					if (HisUnit * pHisUnit = faceOff.His()->IsHisUnit())
						if (pHisUnit->Unit()->isVisible())
							if ((u->Flying() &&
									(
									!faceOff.HisAttack() && !faceOff.His()->Is(Protoss_Zealot)
									
									// ||
									//pHisUnit->Is(Terran_Marine)
									)
								)
								||
								(!u->Flying() &&
									(
									!faceOff.HisAttack() &&
										!(pHisUnit->Is(Zerg_Overlord) && ai()->GetStrategy()->Detected<ZerglingRush>()) &&
										!pHisUnit->Is(Zerg_Cocoon) &&
										!pHisUnit->Is(Zerg_Egg) &&
										!pHisUnit->Is(Zerg_Larva) ||

									pHisUnit->Is(Terran_SCV) ||
									pHisUnit->Is(Terran_Firebat) ||
									pHisUnit->Is(Terran_Marine) ||
								//	pHisUnit->Is(Terran_Medic) ||
									(pHisUnit->Is(Terran_Ghost) && pHisUnit->Unit()->isDetected()) || 
									(pHisUnit->Is(Terran_Vulture_Spider_Mine) && pHisUnit->Unit()->isDetected()) || 

									(pHisUnit->Is(Protoss_Probe) || pHisUnit->Is(Protoss_Zealot)) &&
										!(u->Is(Terran_Siege_Tank_Tank_Mode) && !ai()->GetStrategy()->Active<Walling>()) &&
										(!u->Is(Terran_Marine) || contains(me().EnlargedAreas(), pHisUnit->GetArea(check_t::no_check))) || 
									pHisUnit->Is(Protoss_Archon) || 
								//	pHisUnit->Is(Protoss_High_Templar) || 
								//	pHisUnit->Is(Protoss_Dark_Archon) || 
									(pHisUnit->Is(Protoss_Dark_Templar) && pHisUnit->Unit()->isDetected()) || 

									pHisUnit->Is(Zerg_Drone) || 
									pHisUnit->Is(Zerg_Zergling) || 
									pHisUnit->Is(Zerg_Broodling) || 
									pHisUnit->Is(Zerg_Infested_Terran) || 
									pHisUnit->Is(Zerg_Ultralisk) || 
									pHisUnit->Is(Zerg_Lurker) || 
									pHisUnit->Is(Zerg_Hydralisk) ||

									(pHisUnit->Flying() && pHisUnit->Unit()->isDetected() && u->Is(Terran_Goliath) &&
										!(u->Is(Terran_Goliath) && pHisUnit->Is(Zerg_Mutalisk) && me().Army().KeepGoliathsAtHome()))
									)
								))
								Targets.push_back(&faceOff);

	if (Targets.empty()) return nullptr;
	const FaceOff * fo = selectTarget(Targets);
	if (!fo) return nullptr;
	HisUnit * pTarget = fo->His()->IsHisUnit();

//	if (pTarget->Type().isWorker())
		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (u->Is(Terran_Marine))
				if (!pTarget->Flying())
				{
					if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(u->Pos()))))
					{
					///	bw << u->NameWithId() << "do not attack !!!" << endl;
						return nullptr;
					}

					if (pTarget->Type().isWorker())
						if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(pTarget->Pos()))))
						{
						///	bw << u->NameWithId() << "do not attack worker !!!" << endl;
							return nullptr;
						}
				}

	return pTarget;
}



} // namespace iron



