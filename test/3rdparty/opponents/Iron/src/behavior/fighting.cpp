//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "fighting.h"
#include "walking.h"
#include "fleeing.h"
#include "kiting.h"
#include "sieging.h"
#include "healing.h"
#include "laying.h"
#include "defaultBehavior.h"
#include "../units/factory.h"
#include "../units/barracks.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

static vector<TilePosition> findHigherPlaces(Position from, int tileRadius)
{
	vector<TilePosition> HigherPlaces;

	const TilePosition here(from);
	const int groundHeightHere = ai()->GetMap().GetTile(here).GroundHeight();

	const vector<MyUnit *>  MyUnitsAround  = ai()->GetGridMap().GetMyUnits (ai()->GetMap().Crop(here-tileRadius), ai()->GetMap().Crop(here+tileRadius));
	const vector<HisUnit *> HisUnitsAround = ai()->GetGridMap().GetHisUnits(ai()->GetMap().Crop(here-tileRadius), ai()->GetMap().Crop(here+tileRadius));

	vector<BWAPIUnit *> UnitsAround;
	for (auto u : MyUnitsAround)  if (!u->Flying()) UnitsAround.push_back(u);
	for (auto u : HisUnitsAround) if (!u->Flying()) UnitsAround.push_back(u);

	for (TilePosition dir : { TilePosition(-1, -1), TilePosition(0, -1), TilePosition(+1, -1),
								TilePosition(-1,  0),                      TilePosition(+1,  0),
								TilePosition(-1, +1), TilePosition(0, +1), TilePosition(+1, +1)})
	{
		TilePosition t = here;
		for (int tileDist = 1 ; tileDist <= tileRadius ; ++tileDist)
		{
			t += dir;
			if (!ai()->GetMap().Valid(t)) break;
			const auto & tile = ai()->GetMap().GetTile(t);

			if (tile.MinAltitude() < 15) break;
			if (tile.GetNeutral() || ai()->GetVMap().GetBuildingOn(tile) || !tile.Walkable() || tile.Doodad()) break;
			if (tile.GroundHeight() < groundHeightHere) break;

			if (any_of(UnitsAround.begin(), UnitsAround.end(), [t](BWAPIUnit * u) { return t == TilePosition(u->Pos()); }))
				break;

			if (tile.GroundHeight() > groundHeightHere)
			{
				HigherPlaces.push_back(t);
				break;
			}
		}
	}

	return HigherPlaces;
/*
	if (Agent()->CoolDown() > 20)
	{

		for (int dy = -tileRadius ; dy <= +tileRadius ; ++dy)
		for (int dx = -tileRadius ; dx <= +tileRadius ; ++dx)
		{
			TilePosition t = TilePosition(Agent()->Pos()) + TilePosition(dx, dy);
			if (ai()->GetMap().Valid(t)
		}

		int groundHeightHere = ai()->GetMap().GetTile(TilePosition(Agent()->Pos())).GroundHeight();
		for (TilePosition delta : { TilePosition(-1, -1), TilePosition(0, -1), TilePosition(+1, -1),
									TilePosition(-1,  0),                      TilePosition(+1,  0),
									TilePosition(-1, +1), TilePosition(0, +1), TilePosition(+1, +1)})
		{
			TilePosition there = TilePosition(Agent()->Pos()) + delta*2;
			if (ai()->GetMap().GetTile(there).Doodad()) continue;
			int groundHeightThere = ai()->GetMap().GetTile(there).GroundHeight();
			if (groundHeightThere > groundHeightHere)
			{
				Agent()->Move(center(there));
				bw << Agent()->NameWithId() << " move to BetterPlace" << endl;
				ai()->SetDelay(500);
				return true;
			}
		}
	}

	return false;
*/
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fighting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Fighting *> Fighting::m_Instances;

Fighting::Fighting(MyUnit * pAgent, Position where, zone_t zone, int fightSimResult, bool protectTank)
	: Behavior(pAgent, behavior_t::Fighting), m_zone(zone), m_where(where), m_fightSimResult(fightSimResult), m_protectTank(protectTank)
{
	assert_throw(!protectTank || zone == zone_t::ground);

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	bw << pAgent->NameWithId() << "fight !" << endl;
///	ai()->SetDelay(500);
}


Fighting::~Fighting()
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


void Fighting::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Fighting::StateName() const
{CI(this);
	string s = m_protectTank ? " (protectTank)" : "";

	switch(State())
	{
	case reachingArea:	return "reachingArea" + s;
	case attacking:		return "attacking + s";
	case retreating:	return "retreating + s";
	default:			return "?";
	}
}

/*
bool Fighting::EnemyInRange() const
{
	for (const FaceOff & faceOff : Agent()->FaceOffs())
		if (HisUnit * pTarget = faceOff.His()->IsHisUnit())
			if (faceOff.MyAttack() > 0)
				if (faceOff.DistanceToMyRange() == 0)
					if (!pTarget->InFog() && pTarget->Unit()->exists())
						if (!pTarget->Is(Zerg_Egg))
						if (!pTarget->Is(Zerg_Larva))
							return true;

	return false;
}
*/

HisUnit * Fighting::ChooseTarget() const
{
	multimap<int, HisUnit *> Candidates;
/*
	bool favorWeakest = false;
	if (him().IsProtoss())
		if (Agent()->Is(Terran_Marine))
		{
			favorWeakest = true;
			for (const FaceOff & faceOff : Agent()->FaceOffs())
				if (HisUnit * pTarget = faceOff.His()->IsHisUnit())
					if (faceOff.DistanceToMyRange() < 3*32)
						if (!pTarget->Is(Protoss_Dragoon))
						{
							favorWeakest = false;
							break;
						}
		}
*/
	for (const FaceOff & faceOff : Agent()->FaceOffs())
		if (HisUnit * pTarget = faceOff.His()->IsHisUnit())
			if (faceOff.MyAttack() > 0)
				if (!pTarget->InFog() && pTarget->Unit()->exists())
					//if (!pTarget->Type().isWorker())
					if (!pTarget->Is(Zerg_Egg))
					if (!pTarget->Is(Zerg_Larva))
					if (!ProtectTank() || (pTarget->GroundRange() <= 3*32) /*|| (pTarget->Is(Protoss_Dragoon))*/)
					{
						int score = pTarget->LifeWithShields();

						int distScore = faceOff.DistanceToMyRange();
						if (him().IsProtoss())
							if (Agent()->Is(Terran_Marine))
								distScore = distScore*2 + 50;
						//if (favorWeakest) distScore /= 5;

						score += distScore;

						if (ProtectTank())
							if (dist(Where(), faceOff.His()->Pos()) > 4*32 + faceOff.His()->GroundRange()) score += 1000;

						if (Agent()->Is(Terran_SCV)) score += static_cast<int>(1000 * pTarget->Speed());
						if (pTarget->Is(Protoss_Reaver)) score -= 300;
						if (pTarget->Is(Protoss_Dark_Templar)) score -= 300;
						
						if (pTarget->Type().isWorker() && !Agent()->Is(Terran_Vulture))
							if (faceOff.DistanceToMyRange() >= 0)
								score += 1000;

						Candidates.emplace(score, pTarget);
					}

	if (Candidates.empty()) return nullptr;

	return Candidates.begin()->second;
}


void Fighting::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pTarget == other)
		m_pTarget = nullptr;
}


void Fighting::OnFrame_v()
{CI(this);
#if DEV
	if ((m_state == attacking) && m_pTarget)
	{
		drawLineMap(Agent()->Pos(), m_pTarget->Pos(), GetColor());//1
		bw->drawCircleMap(m_pTarget->Pos(), 18, GetColor());
	}
	if (m_state == retreating)
	{
		drawLineMap(Agent()->Pos(), m_retreatingPos, GetColor());//1
		bw->drawTriangleMap(Agent()->Pos() + Position(0, -20), Agent()->Pos() + Position(-15, +10), Agent()->Pos() + Position(+15, +10), GetColor(), true);
	}
#endif

	if (!Agent()->CanAcceptCommand()) return;

	auto Threats2 = findThreats(Agent(), 2*32);
	if (any_of(Threats2.begin(), Threats2.end(), [this](const FaceOff * fo)
		{
			return fo->His()->Is(Terran_Vulture_Spider_Mine) ||
					fo->His()->Is(Protoss_Reaver) ||
					fo->His()->GroundThreatBuilding() && fo->His()->Completed() ||
					Agent()->IsMy<Terran_Medic>();
		}))

		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (!Agent()->CanMove()) return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (Sieging::EnterCondition(Agent()))
		return Agent()->ChangeBehavior<Sieging>(Agent()->As<Terran_Siege_Tank_Tank_Mode>());


	switch (State())
	{
	case reachingArea:		OnFrame_reachingArea(); break;
	case attacking:			OnFrame_attacking(); break;
	case retreating:		OnFrame_retreating(); break;
	default: assert_throw(false);
	}
}


void Fighting::OnFrame_reachingArea()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if ((Zone() == zone_t::air) && (Agent()->Zone() != zone_t::air))
		return ChangeState(attacking);

	int distToDest = Agent()->Flying()
					? roundedDist(Agent()->Pos(), Where())
					: groundDist(Agent()->Pos(), Where());

	if ((distToDest < 15*32) && ChooseTarget())
	{
		if (GetSubBehavior()) ResetSubBehavior();
		return ChangeState(attacking);
	}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		if (Agent()->Flying())
			return Agent()->Move(Where());
		else
			return SetSubBehavior<Walking>(Agent(), Where(), __FILE__ + to_string(__LINE__));
	}

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());


	if (My<Terran_Medic> * pMedic = Agent()->IsMy<Terran_Medic>())
		if (Healing::FindTarget(pMedic))
			return Agent()->ChangeBehavior<Healing>(pMedic);

	if (ai()->Frame() - m_inStateSince > 5)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


void Fighting::OnFrame_retreating()
{CI(this);
	int distToDest = roundedDist(Agent()->Pos(), m_retreatingPos);

	if ((distToDest < 4*32) || (Agent()->Life() < Agent()->PrevLife(35)))
	{
		if (GetSubBehavior()) ResetSubBehavior();
		return ChangeState(attacking);
	}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return Agent()->Move(m_retreatingPos);
	}

	if (ai()->Frame() - m_inStateSince > 10)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


bool Fighting::CheckRetreating()
{CI(this);
	if (him().IsProtoss())
//		if (m_fightSimResult < 1000)
		if (me().Units(Terran_Marine).size() < him().AllUnits(Protoss_Dragoon).size()*2.5)
		{
			if (Zone() == zone_t::ground)
				if (Agent()->Is(Terran_Marine) || Agent()->Is(Terran_Medic))
				{
					const int dist_agent_fightSpot = roundedDist(Agent()->Pos(), Where());

					MyUnit * pNearestCoveringSiegedTank = nullptr;
					int minDist_agent_CoveringsiegedTank = 15*32;
					for (const auto & tank : me().Units(Terran_Siege_Tank_Tank_Mode))
						if (tank->Completed() && !tank->Loaded())
							if (tank->Is(Terran_Siege_Tank_Siege_Mode) || tank->GetBehavior()->IsSieging())
							{
								const int dist_tank_fightSpot = roundedDist(tank->Pos(), Where());
								if (dist_tank_fightSpot > dist_agent_fightSpot)
								{
									int d = roundedDist(tank->Pos(), Agent()->Pos());
									if (d < minDist_agent_CoveringsiegedTank)
									{
										minDist_agent_CoveringsiegedTank = d;
										pNearestCoveringSiegedTank = tank.get();
									}
								}
							}

					if (pNearestCoveringSiegedTank)
						if (roundedDist(pNearestCoveringSiegedTank->Pos(), Agent()->Pos()) > 5*32)
							if ((Agent()->Life() == Agent()->PrevLife(35)))
							{
								m_retreatingPos = pNearestCoveringSiegedTank->Pos();
								return true;
							}
				}
		}
		//else bw->drawCircleScreen(Position(50, 50), 20, Colors::Green, true);

	return false;

}

void Fighting::OnFrame_attacking()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		m_lastMinePlacement = 0;
	}

	/*if (Agent()->Flying())
	{
		if (Kiting::KiteCondition(Agent()))
			return Agent()->ChangeBehavior<Kiting>(Agent());
	}
	else*/
	
	// if (!ProtectTank())

	bool reaverHere = false;
	{
		auto Threats5 = findThreats(Agent()->IsMyUnit(), 5*32);
		bool enemyTargetedByMines = false;
		for (const auto * pFaceOff : Threats5)
			if (auto * pHisUnit = pFaceOff->His()->IsHisUnit())
			{
				if (pHisUnit->Is(Protoss_Reaver) || pHisUnit->Is(Protoss_Scarab)) { reaverHere = true; break; }

				if (pFaceOff->GroundDistanceToHitHim() < 6*32)
					for (Position minePos : pHisUnit->MinesTargetingThis())
						if (roundedDist(minePos, Agent()->Pos()) < roundedDist(minePos, pHisUnit->Pos()))
						{
							enemyTargetedByMines = true;
							break;
						}
			}
		if (enemyTargetedByMines || reaverHere) return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	if (CheckRetreating()) return ChangeState(retreating);

	if (ProtectTank())
	{
		if (!him().IsProtoss())
			if (Agent()->Life() < Agent()->PrevLife(1))
				return Agent()->ChangeBehavior<Kiting>(Agent());
	}

//	if (ai()->Frame() - m_lastMinePlacement < 16) return;

	if (Agent()->CoolDown() > 0) m_lastShot = ai()->Frame();

	if (me().HasResearched(TechTypes::Stim_Packs))
		if (Agent()->Unit()->canUseTechWithoutTarget(TechTypes::Stim_Packs))
			if (Agent()->Life() >= 31)
				if (!Agent()->Unit()->isStimmed())
					return Agent()->StimPack();
/*
	if (reaverHere)
		if (Agent()->Is(Terran_Marine))
		{
			int minDist = 2*32;
			MyUnit * pClosestBio = nullptr;
			Position groupCenter = {0, 0};
			int groupSize = 0;
			for (UnitType t : {Terran_Marine})
				for (const auto & m : me().Units(t))
					if (m.get() != Agent())
						if (m->GetBehavior()->IsFighting() || m->GetBehavior()->IsHealing())
						{
							int d = roundedDist(Agent()->Pos(), m->Pos());
							if (d < 5*32)
							{
								groupCenter += m->Pos();
								++groupSize;
								if (d < minDist)
								{
									minDist = d;
									pClosestBio = m.get();
								}
							}
						}
			if (groupSize) groupCenter /= groupSize;

			if (pClosestBio)
			{
				Vect dir = toVect(Agent()->Pos() - (groupCenter + pClosestBio->Pos())/2);
				dir.Normalize();
				Position delta = toPosition(dir * 12);

				Position dest = Agent()->Pos() + delta;
				TilePosition t(dest);
				if (ai()->GetMap().Valid(t))
				{
					const auto & tile = ai()->GetMap().GetTile(t);
					if (!tile.GetNeutral() && !ai()->GetVMap().GetBuildingOn(tile) && tile.Walkable() && !tile.Doodad())
					{
						m_pLastAttackedTarget = nullptr;
						Agent()->Move(dest, check_t::no_check);
#if DEV
					///	drawLineMap(Agent()->Pos(), dest, Colors::Green);
					///	bw->drawCircleMap(Agent()->Pos(), 20, Colors::Green, true);
					///	bw->drawCircleMap(dest, 3, Colors::Green, true);
#endif
						return;
					}
				}
			}
		}
*/

	if (Agent()->Is(Terran_Medic))
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
/*
	if (him().IsProtoss())
		if (Agent()->Is(Terran_Marine))
			if (Agent()->Life() <= 10)
				//if (Agent()->Life() <= Agent()->PrevLife(5) - 10)
				{
					bw << Agent()->NameWithId() << " flies !! " << Agent()->Life() << endl;
					//ai()->SetDelay(500);
					//bw->drawCircleMap(Agent()->Pos(), 10, Colors::Black, true);
					return Agent()->ChangeBehavior<Kiting>(Agent());
			}
*/


	if ((Agent()->CoolDown() <= 3) || (Agent()->CoolDown() % 10 == 0))
	{
		m_pTarget = ChooseTarget();
	
		if (!m_pTarget)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

//		bw->drawCircleMap(Agent()->Pos(), 18, Colors::Red, true);
/*
		if (me().HasResearched(TechTypes::Spider_Mines))
			if (auto * pVulture = Agent()->IsMy<Terran_Vulture>())
				if (pVulture->RemainingMines() >= 1)
					if (m_pTarget->Is(Terran_Siege_Tank_Siege_Mode) ||
						m_pTarget->Is(Terran_Siege_Tank_Tank_Mode) ||
						m_pTarget->Is(Terran_Goliath) ||
						m_pTarget->Is(Protoss_Dragoon))
					{
						m_lastMinePlacement = ai()->Frame();
						return pVulture->PlaceMine(pVulture->Pos());
					}
*/
		if (m_pLastAttackedTarget != m_pTarget)
		{
			m_pLastAttackedTarget = m_pTarget;
			if (him().IsProtoss() && Agent()->Is(Terran_Marine))
				Agent()->Patrol(m_pTarget->Pos(), check_t::no_check);
			else
				Agent()->Attack(m_pTarget, check_t::no_check);
			return;
		}

		if (ai()->Frame() - m_inStateSince > 10)
			if (ai()->Frame() - m_lastShot > 15)
				if (ai()->Frame() - Agent()->LastFrameMoved() > 15)
					return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (m_pTarget)
		if ((Agent()->CoolDown() > 0))// || Agent()->Is(Terran_Marine) && m_pTarget->Is(Protoss_Dragoon))
		{
			if (!Agent()->Type().isOrganic())
			if (!Agent()->Is(Terran_Siege_Tank_Tank_Mode))
				if (m_pTarget->Unit()->isMoving())
				{
					const double dist0 = dist(Agent()->PrevPos(0), m_pTarget->PrevPos(0));
					const double dist1 = dist(Agent()->PrevPos(1), m_pTarget->PrevPos(1));
					const double dist2 = dist(Agent()->PrevPos(2), m_pTarget->PrevPos(2));
					const double deltaDist0 = dist0 - dist1;
					const double deltaDist1 = dist1 - dist2;
					if (deltaDist0 > deltaDist1)
					{
						const int deltaDistWhenCanFire = int(0.5 + deltaDist0 * Agent()->CoolDown());
						const int distToRangeWhenCanFire = FaceOff(Agent(), m_pTarget).DistanceToMyRange() + deltaDistWhenCanFire;

						if (distToRangeWhenCanFire > -Agent()->CoolDown()/2 - 5)
						{
							m_pLastAttackedTarget = nullptr;
							Agent()->Patrol(m_pTarget->Pos(), check_t::no_check);
						///	bw->drawCircleMap(Agent()->Pos(), 20, Colors::Black, true);
						///	bw->drawTextMap(Agent()->Pos(), "%c%s", Text::White, (to_string(distToRangeWhenCanFire) + " > " + to_string(-Agent()->CoolDown()/2 - 5)).c_str());
							return;
						}
					}
				}

			if (!Agent()->Type().isOrganic())
				if (!ProtectTank())
					if (Agent()->CoolDown() > 5)
						if (!m_pTarget->Unit()->isMoving())
						{
							vector<TilePosition> HigherPlaces = findHigherPlaces(Agent()->Pos(), 3);
							if (!HigherPlaces.empty())
							{
								///ai()->SetDelay(2000);
								for (TilePosition hp : HigherPlaces)
									if (dist(center(hp), m_pTarget->Pos()) < Agent()->GroundRange())
									{
#if DEV
										bw->drawBoxMap(center(hp)-13, center(hp)+13, Colors::White, true);
										drawLineMap(center(hp), Agent()->Pos(), Colors::White);
#endif
										m_pLastAttackedTarget = nullptr;
										Agent()->Move(center(hp), check_t::no_check);
										return;
									}
							}
						}

			if (!ProtectTank())
				if (auto * pVulture = Agent()->IsMy<Terran_Vulture>())
					if (!m_pTarget->Is(Terran_Vulture))
						if (Agent()->Life() < Agent()->PrevLife(10))
							if (Agent()->Life() < 40)
								if (Agent()->CoolDown() > 10)
									if (me().HasResearched(TechTypes::Spider_Mines))
										if (pVulture->RemainingMines() > 1)
										{
										///	ai()->SetDelay(500);
										///	bw << Agent()->NameWithId() << " Laying mine before dying" << endl;
											return Agent()->ChangeBehavior<Laying>(Agent());
										}
		}

}



} // namespace iron



