//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "sieging.h"
#include "repairing.h"
#include "fighting.h"
#include "sniping.h"
#include "checking.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/berserker.h"
#include "../strategy/walling.h"
#include "../territory/vgridMap.h"
#include "../units/factory.h"
#include "../units/army.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

bool Sieging::EnterCondition(const MyUnit * u)
{
	if (me().HasResearched(TechTypes::Tank_Siege_Mode))
		if (const My<Terran_Siege_Tank_Tank_Mode> * pTank = u->IsMy<Terran_Siege_Tank_Tank_Mode>())
		{
//			if (him().HydraPressure_needVultures())
//				if (u->GetArea() != me().GetArea())
//					return false;

			const bool wallDefense =
				him().IsProtoss() && me().Army().KeepTanksAtHome() &&
						(ai()->GetStrategy()->Active<Walling>() ||
						ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(u->Pos()))));

			int distSiegeTarget = numeric_limits<int>::max();
			HisBWAPIUnit * pSiegeTarget = nullptr;

			for (const FaceOff & faceOff : u->FaceOffs())
				if (faceOff.MyAttack() > 0)
					if (( faceOff.His()->InFog() && faceOff.His()->ThreatBuilding()) ||
						(!faceOff.His()->InFog() && faceOff.His()->Unit()->exists()))
								if (faceOff.His()->ThreatBuilding() ||
									faceOff.His()->Is(Terran_Ghost) ||
									faceOff.His()->Is(Terran_Goliath) ||
									faceOff.His()->Is(Terran_Vulture) ||
									faceOff.His()->Is(Terran_Siege_Tank_Tank_Mode) ||
									faceOff.His()->Is(Protoss_Dark_Archon) ||
									faceOff.His()->Is(Protoss_Dragoon) ||
									faceOff.His()->Is(Protoss_Reaver) ||
									faceOff.His()->Is(Zerg_Defiler) ||
									faceOff.His()->Is(Zerg_Hydralisk) ||
									faceOff.His()->Is(Zerg_Lurker) ||
									wallDefense)
									if (int dist = pTank->CanSiegeAttack(faceOff.His()))
										if (!pSiegeTarget || (dist < distSiegeTarget))
										{
											if (faceOff.His()->InFog() && !Checking::FindCandidate(faceOff.His()))
												continue;

											pSiegeTarget = faceOff.His();
											distSiegeTarget = dist;
										}


			TilePosition vicinityTopLeft = TilePosition(u->Pos());
			TilePosition vicinityBottomRight = vicinityTopLeft;
			if (pSiegeTarget) makeBoundingBoxIncludePoint(vicinityTopLeft, vicinityBottomRight, TilePosition(pSiegeTarget->Pos()));
			
			vicinityTopLeft = ai()->GetMap().Crop(TilePosition(vicinityTopLeft)-7);
			vicinityBottomRight = ai()->GetMap().Crop(TilePosition(vicinityBottomRight)+7);

			vector<MyUnit *> MyUnitsAround = ai()->GetGridMap().GetMyUnits(vicinityTopLeft, vicinityBottomRight);
			int myVulturesAndGoliathsAndMarinesAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [](MyUnit * u){ return u->Is(Terran_Marine) || u->Is(Terran_Vulture) || u->Is(Terran_Goliath); });
			int myTanksAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [](MyUnit * u){ return u->Is(Terran_Siege_Tank_Tank_Mode) || u->Is(Terran_Siege_Tank_Siege_Mode); });
			int mySnipersAround = count_if(Sniping::Instances().begin(), Sniping::Instances().end(), [u](Sniping * s){ return dist(s->Agent()->Pos(), u->Pos()) < 5*32; });

			vector<HisUnit *> HisUnitsAround = ai()->GetGridMap().GetHisUnits(vicinityTopLeft, vicinityBottomRight);
			int hisVulturesAndGoliathsAround = count_if(HisUnitsAround.begin(), HisUnitsAround.end(), [](HisUnit * u){ return u->Is(Terran_Vulture) || u->Is(Terran_Goliath); });
			int hisZealotsAround = count_if(HisUnitsAround.begin(), HisUnitsAround.end(), [](HisUnit * u){ return u->Is(Protoss_Zealot); });
			int hisDragoonsAround = count_if(HisUnitsAround.begin(), HisUnitsAround.end(), [](HisUnit * u){ return u->Is(Protoss_Dragoon); });
			int hisZerglingsAround = count_if(HisUnitsAround.begin(), HisUnitsAround.end(), [](HisUnit * u){ return u->Is(Zerg_Zergling); });

			if (pSiegeTarget && pSiegeTarget->Is(Terran_Vulture))
				if (!((myVulturesAndGoliathsAndMarinesAround >= 2) && (myVulturesAndGoliathsAndMarinesAround > hisVulturesAndGoliathsAround)))
					return false;

			if (pSiegeTarget && pSiegeTarget->Is(Protoss_Dragoon))
			{
				if (!wallDefense)
				{
					if (hisZealotsAround)
						if (!(myVulturesAndGoliathsAndMarinesAround > 3 + 2*hisZealotsAround))
							return false;

					if (!contains(me().EnlargedAreas(), u->GetArea()))
						if (myVulturesAndGoliathsAndMarinesAround*2 + myTanksAround < hisDragoonsAround)
						if (!(myTanksAround >= (hisDragoonsAround+1)/2))
							return false;
				}
			}

			if (!(him().IsTerran() || (pSiegeTarget && pSiegeTarget->Is(Zerg_Hydralisk) && !hisZerglingsAround)))
				for (MyUnit * w : MyUnitsAround)
					if (w->Is(Terran_Vulture_Spider_Mine))
						if (dist(u->Pos(), w->Pos()) < 3*32)
							return false;

			bool stochasticSiege = false;
			if (!pSiegeTarget)
				if (!him().IsProtoss())
				if (!hisZealotsAround)
					if (!him().HydraPressure_needVultures())
						if (!u->GetBehavior()->IsRaiding())
							if ((myVulturesAndGoliathsAndMarinesAround >= 2) && (rand() % 100 == 0))
								stochasticSiege = true;

			if (pSiegeTarget)
			{
				if (!u->GetBehavior()->IsGuarding())
				if (!wallDefense)
					for (const FaceOff & faceOff : pTank->FaceOffs())
						if (!faceOff.His()->InFog())
							if (faceOff.His()->Unit()->exists())
								if (faceOff.HisAttack())
		//							if ((faceOff.DistanceToHisRange() < 7*32) && (faceOff.AirDistance() < 10*32))
									if ((faceOff.DistanceToHisRange() < 7*32) && (faceOff.AirDistanceToHitMe() < 10*32))
									if (faceOff.His()->Is(Terran_Marine) ||
		//								faceOff.His()->Is(Terran_Vulture) ||
										faceOff.His()->Is(Zerg_Zergling) ||
										faceOff.His()->Is(Zerg_Hydralisk) &&
											((faceOff.DistanceToHisRange() < 5*32) ||
											((myVulturesAndGoliathsAndMarinesAround + 2*myTanksAround + mySnipersAround) < (int)pTank->FaceOffs().size())) ||
										faceOff.His()->Is(Zerg_Ultralisk) ||
										faceOff.His()->Is(Protoss_Zealot) &&
											((faceOff.DistanceToHisRange() < 5*32) ||
											((myVulturesAndGoliathsAndMarinesAround + 2*myTanksAround + mySnipersAround) < (int)pTank->FaceOffs().size())) ||
										faceOff.His()->Is(Protoss_Dark_Templar) ||
										faceOff.His()->Is(Protoss_High_Templar) ||
										faceOff.His()->Is(Protoss_Archon) ||
										faceOff.His()->Is(Protoss_Dragoon) && (faceOff.DistanceToHisRange() < 0) ||
										faceOff.His()->Is(Zerg_Lurker) && ((faceOff.DistanceToHisRange() < 2*32) || (faceOff.His()->Unit()->isMoving())) ||
										faceOff.His()->Type().isWorker())
											return false;

				return true;
			}
			
			if (stochasticSiege)
			{
			///	ai()->SetDelay(50);
			///	bw << u->NameWithId() << "stochastic siege!" << endl;
				return true;
			}
		}

	return false;
}


bool Sieging::LeaveCondition(const vector<MyBWAPIUnit *> & MyUnitsAround) const
{
	if (auto s = ai()->GetStrategy()->Active<Berserker>())
		if (ai()->Frame() - s->ActiveSince() < 10)
			if (me().MineralsAvailable() > 5000)
			if (me().GasAvailable() > 1500)
				return true;

	if (him().IsProtoss() && me().Army().KeepTanksAtHome()/* &&
		(ai()->GetStrategy()->Active<Walling>() ||
		ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))*/)
		return false;


	if (!Agent()->CoolDown()) return false;
	if (Agent()->CoolDown() > 70) return false;

	int countMyVulturesAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [this](MyBWAPIUnit * u)
										{ return u->Is(Terran_Vulture) && u->GetBehavior()->IsKiting() && squaredDist(Agent()->Pos(), u->Pos()) < 6*32 * 6*32; });
	int countMyGoliathsAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [this](MyBWAPIUnit * u)
										{ return u->Is(Terran_Goliath) && u->GetBehavior()->IsKiting() && squaredDist(Agent()->Pos(), u->Pos()) < 6*32 * 6*32; });
	int countMyTanksAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [this](MyBWAPIUnit * u)
										{ return u->Is(Terran_Siege_Tank_Tank_Mode) && u->GetBehavior()->IsKiting() && squaredDist(Agent()->Pos(), u->Pos()) < 6*32 * 6*32; });
	int countMySiegeTanksAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [this](MyBWAPIUnit * u)
										{ return u->Is(Terran_Siege_Tank_Siege_Mode) && u->GetBehavior()->IsSieging() && squaredDist(Agent()->Pos(), u->Pos()) < 8*32 * 8*32; });
	int countMyMarinesAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [this](MyBWAPIUnit * u)
										{ return u->Is(Terran_Marine) && squaredDist(Agent()->Pos(), u->Pos()) < 6*32 * 6*32; });

	double takingDamageRate = 0;

	for (const FaceOff & faceOff : Agent()->FaceOffs())
	{
		if (!faceOff.His()->InFog())
			if (faceOff.His()->Unit()->exists())
				if (faceOff.HisAttack())
				{
					if (faceOff.His()->Flying())
					{
						if (Agent()->Life() < Agent()->PrevLife(10))
							if (faceOff.DistanceToHisRange() < 1*32)
								if (none_of(me().Buildings(Terran_Missile_Turret).begin(), me().Buildings(Terran_Missile_Turret).end(),
									[&faceOff](const unique_ptr<MyBuilding> & b){ return dist(b->Pos(), faceOff.His()->Pos()) < b->AirRange(); }))
								{
									takingDamageRate += 1.0 / max(1, faceOff.FramesToKillMe());
								}
					}
					else
					{
						if ((faceOff.DistanceToHisRange() < 6*32) && (faceOff.AirDistanceToHitHim() < 9*32))
						{
							frame_t k = faceOff.FramesToKillMe();
							switch (faceOff.His()->Type())
							{
							case Terran_SCV: case Protoss_Probe: case Zerg_Drone: 
								k = 10000;
								if (faceOff.DistanceToMyRange() < 2*32)
									if (countMyVulturesAround + countMyGoliathsAround + countMyTanksAround == 0)
										k = 1;
								break;
							case Zerg_Lurker: 
								k = 10000;
								if (!faceOff.His()->Unit()->isVisible())
									if (faceOff.DistanceToHisRange() < 0)
										k = 1;
								break;
							case Protoss_Dark_Templar:
								k = 1;
								break;
							case Zerg_Zergling:
								k = frame_t(k * (1.0 + (countMyVulturesAround + countMyGoliathsAround + countMyTanksAround)/1.2));
								break;

							case Terran_Marine:
								k = frame_t(k * (1.0 + (countMyVulturesAround + countMyGoliathsAround + countMyTanksAround + countMySiegeTanksAround)/1.5));
								break;
							case Protoss_Zealot:
								k = frame_t(k * (1.0 + (countMyVulturesAround + countMyGoliathsAround + countMyTanksAround + countMyMarinesAround/2)/8.0));
								break;
							case Zerg_Hydralisk:
								k = frame_t(k * (1.0 + (countMyVulturesAround/2 + countMyGoliathsAround + countMyTanksAround*2 + countMySiegeTanksAround*2)/6.0));
								break;
							case Zerg_Ultralisk:
								k = frame_t(k * (1.0 + (countMyVulturesAround/4 + countMyGoliathsAround + countMyTanksAround*4)/16.0));
								break;
							case Protoss_Archon:
								k = frame_t(k * (1.0 + (countMyVulturesAround/2 + countMyGoliathsAround + countMyTanksAround*4)/16.0));
								break;
//							case Protoss_Dragoon:
//								k = frame_t(k * (1.0 + (countMyVulturesAround/4 + countMyGoliathsAround + countMyTanksAround*2 + countMySiegeTanksAround*4)/8.0));
//								break;
							default:
								k = 10000;
							}
							takingDamageRate += 1.0 / max(1, k);
						}
					}
				}
	}

	frame_t framesToBeKilled = abs(takingDamageRate) < 0.000001 ? 1000000 : frame_t(1.0/takingDamageRate);
//	bw->drawTextMap(Agent()->Pos().x-15, Agent()->Pos().y-8, "%c%d-%d", Text::White, framesToBeKilled, Agent()->CoolDown());
//	bw << framesToBeKilled << " - " <<  Agent()->CoolDown() << " = " << framesToBeKilled - Agent()->CoolDown() << endl;
	if (framesToBeKilled < 75) return false;	// no time to flee
	if (framesToBeKilled - Agent()->CoolDown() > 100) return false;	// there is still time for firing
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Sieging
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Sieging *> Sieging::m_Instances;


Sieging::Sieging(My<Terran_Siege_Tank_Tank_Mode> * pAgent)
	: Behavior(pAgent, behavior_t::Sieging)
{
	assert_throw(pAgent->Is(Terran_Siege_Tank_Tank_Mode));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
	m_delayUntilUnsieging = him().IsTerran() ? 300 :
							me().Army().KeepTanksAtHome() ? 200 :
							100;
///	ai()->SetDelay(100);
}


Sieging::~Sieging()
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


void Sieging::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();

	m_staySiegedUntil = 0;
}


string Sieging::StateName() const
{CI(this);
	switch(State())
	{
	case sieging:			return m_staySiegedUntil ? "sieging (" + to_string(m_staySiegedUntil - ai()->Frame()) + ")" : "sieging";
	case unsieging:			return "unsieging";
	default:				return "?";
	}
}


void Sieging::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (m_pTarget == other)
		m_pTarget = nullptr;
}


pair<HisBWAPIUnit *, int> Sieging::ChooseTarget(const vector<MyBWAPIUnit *> & MyUnitsAround, const vector<HisBWAPIUnit *> & HisUnitsAround) const
{CI(this);
	for (bool inFog : {false, true})
	{
		vector<const FaceOff *> Candidates;

		for (const FaceOff & faceOff : Agent()->FaceOffs())
			if (faceOff.MyAttack() > 0)
				if (( inFog &&  faceOff.His()->InFog() && faceOff.His()->ThreatBuilding()) ||
					(!inFog && !faceOff.His()->InFog() && faceOff.His()->Unit()->exists()))
					if (!faceOff.His()->Is(Zerg_Egg))
					if (!faceOff.His()->Is(Zerg_Larva))
						if (Agent()->CanSiegeAttack(faceOff.His()))
							if (faceOff.His()->InFog() || Agent()->Unit()->canAttackUnit(faceOff.His()->Unit()))
								Candidates.push_back(&faceOff);

		HisBWAPIUnit * pBestCand = nullptr;
		int maxScore = numeric_limits<int>::min();

		for (const FaceOff * faceOff : Candidates)
		{
			HisBWAPIUnit * cand = faceOff->His();
			int countHisUnitsAround = count_if(HisUnitsAround.begin(), HisUnitsAround.end(), [cand](HisBWAPIUnit * u){ return cand != u && squaredDist(cand->Pos(), u->Pos()) < 40*40; });
			int countMyUnitsAround = count_if(MyUnitsAround.begin(), MyUnitsAround.end(), [cand](MyBWAPIUnit * u){ return squaredDist(cand->Pos(), u->Pos()) < 40*40; });

			int myAttackBonus = int(2*faceOff->MyAttack());				// favor high damage
//			int hisAttackBonus = faceOff->HisAttack();					// tanks are for high damage enemies
			int notDangerousMalus = !faceOff->His()->Is(Zerg_Lurker) &&
									!faceOff->His()->Is(Protoss_High_Templar) &&
									!faceOff->HisAttack() ? 1000 : 0;	// enemies that can't attack are not first priority
			int oneShotBonus = (faceOff->MyAttack() >= cand->LifeWithShields()) ? faceOff->HisAttack() : 0;	// favor enemies that can be killed in one shot
			int hisUnitsAroundBonus = countHisUnitsAround*20;			// favor targets with enemies around (splash damage)
			int myUnitsAroundMalus = countMyUnitsAround*20;				// avoid targets with friens around (splash damage)
			int distanceMalus = (faceOff->DistanceToHisRange() > 0) ? faceOff->DistanceToHisRange() / 16 : 0;	// avoid targets too far
			int inHisRangeBonus = (faceOff->DistanceToHisRange() <= 0) ? faceOff->HisAttack() : 0;				// favor enemies in range
			int lurkerBonus = faceOff->His()->Is(Zerg_Lurker) ? 1000 : 0;										// kill lurkers
			int darkTemplarBonus = faceOff->His()->Is(Protoss_Dark_Templar) ? 1000 : 0;							// kill dark templars
			int reaverBonus = faceOff->His()->Is(Protoss_Reaver) ? 5000 : 0;										// kill reavers

			if (cand->IsHisBuilding())
			{
				if (!faceOff->HisAttack()) continue;
				hisUnitsAroundBonus = 0;
				myUnitsAroundMalus = 0;
				if (distanceMalus && !oneShotBonus) distanceMalus = 500;
			}

			int score = + myAttackBonus
						- notDangerousMalus
						+ oneShotBonus
						+ hisUnitsAroundBonus
						- myUnitsAroundMalus
						- distanceMalus
						+ inHisRangeBonus
						+ lurkerBonus
						+ darkTemplarBonus
						+ reaverBonus;

	#if DEV
	//		bw->drawTextMap(cand->Pos().x-5, cand->Pos().y-8, "%c%d  =  %d - %d + %d + %d - %d - %d + %d + %d", Text::Orange, score,
	//			myAttackBonus, notDangerousMalus, oneShotBonus, hisUnitsAroundBonus, myUnitsAroundMalus, distanceMalus, inHisRangeBonus, lurkerBonus);
			bw->drawTextMap(cand->Pos().x-5, cand->Pos().y-8, "%c%d", Text::Orange, score);
	#endif

			if (score > maxScore)
			{
				maxScore = score;
				pBestCand = cand;
			}
		}

		if (pBestCand) return make_pair(pBestCand, maxScore);
	}

	return make_pair(nullptr, 0);
}


void Sieging::OnFrame_v()
{CI(this);
#if DEV
	if ((m_state == sieging) && m_pTarget)
	{
		drawLineMap(Agent()->Pos(), m_pTarget->Pos(), GetColor());//1
		bw->drawCircleMap(m_pTarget->Pos(), 15, GetColor());


		for (const FaceOff & faceOff : Agent()->FaceOffs())
			if (faceOff.MyAttack() > 0)
				if (faceOff.His()->Is(Terran_Siege_Tank_Siege_Mode))
					if (int dist = Agent()->CanSiegeAttack(faceOff.His()))
					{
						bw->drawCircleMap(faceOff.His()->Pos(), 15, Colors::Blue);
						bw->drawTextMap(faceOff.His()->Pos() + Position(-10, 0), "%c%d", Text::White, dist);
					}

	}
#endif

//	if (!him().IsProtoss())
		for (const FaceOff & faceOff : Agent()->FaceOffs())
			if (!faceOff.His()->InFog())
				if (faceOff.HisAttack())
					if (!faceOff.His()->Flying())
						if (faceOff.His()->GroundRange() <= 3*32)
							if (faceOff.DistanceToHisRange() < 
								(faceOff.His()->Type().isWorker() ? 3*32 : faceOff.His()->Is(Protoss_Dragoon) ? 2*32 : 5*32))
							{
								int tileRadius = him().IsProtoss() && me().Army().KeepTanksAtHome() ? 20 : 5;
								vector<MyUnit *> MyUnitsAround = ai()->GetGridMap().GetMyUnits(ai()->GetMap().Crop(TilePosition(Agent()->Pos())-tileRadius), ai()->GetMap().Crop(TilePosition(Agent()->Pos())+tileRadius));
								for (MyUnit * u : MyUnitsAround)
									if (u->Is(Terran_Vulture) || u->Is(Terran_Marine))
										if (u->Completed())
										if (!u->Loaded())
											if (!u->GetBehavior()->IsDestroying())
											if (!u->GetBehavior()->IsKillingMine())
											if (!u->GetBehavior()->IsLaying())
											if (!u->GetBehavior()->IsFighting())
											if (!u->GetBehavior()->IsSniping())
											if (!u->GetBehavior()->IsWalking())
											{
											///	ai()->SetDelay(500);
											///	bw << u->NameWithId() << " helps attacked " << Agent()->NameWithId() << endl;
												u->ChangeBehavior<Fighting>(u, Agent()->Pos(), zone_t::ground, 1, bool("protectTank"));
											}
								break;
							}

	if (!Agent()->CanAcceptCommand()) return;
/*
	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	auto Threats3 = findThreats(Agent(), 3*32);
	if (!Threats3.empty())
		return Agent()->ChangeBehavior<Fleeing>(Agent());

*/
	switch (State())
	{
	case sieging:		OnFrame_sieging(); break;
	case unsieging:		OnFrame_unsieging(); break;
	default: assert_throw(false);
	}
}


void Sieging::OnFrame_sieging()
{CI(this);
	if (JustArrivedInState())
	{
		m_pTarget = nullptr;
		m_staySiegedUntil = 0;
		m_killsSinceSieging = Agent()->Unit()->getKillCount();
///		ai()->SetDelay(50);
		m_inStateSince = ai()->Frame();
		return Agent()->Siege();
	}

	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
	{
	}
	else
	{
		if (m_staySiegedUntil == 0) m_staySiegedUntil = ai()->Frame() + m_delayUntilUnsieging;

		vector<MyBWAPIUnit *> MyUnitsAround;
		vector<HisBWAPIUnit *> HisUnitsAround;
		{
			int iAgent, jAgent;
			tie(iAgent, jAgent) = ai()->GetGridMap().GetCellCoords(TilePosition(Agent()->Pos()));
			for (int j = jAgent-2 ; j <= jAgent+2 ; ++j)
			for (int i = iAgent-2 ; i <= iAgent+2 ; ++i)
				if (ai()->GetGridMap().ValidCoords(i, j))
				{
					auto & Cell = ai()->GetGridMap().GetCell(i, j);
					MyUnitsAround.insert(MyUnitsAround.end(), Cell.MyUnits.begin(), Cell.MyUnits.end());
					HisUnitsAround.insert(HisUnitsAround.end(), Cell.HisUnits.begin(), Cell.HisUnits.end());
					HisUnitsAround.insert(HisUnitsAround.end(), Cell.HisBuildings.begin(), Cell.HisBuildings.end());
				}
		}

		if (LeaveCondition(MyUnitsAround))
		{
			return ChangeState(unsieging);
		}

		if (double(Agent()->Life()) / Agent()->MaxLife() < Agent()->MinLifePercentageToRepair() - 0.000001)
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				Repairing::GetRepairer(Agent(), 32*32);
/*
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 16*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 16*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 32*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*1) ? 64*32 : 1000000);
*/		
		tie(m_pTarget, m_nextTargetScore) = ChooseTarget(MyUnitsAround, HisUnitsAround);

		if (m_pTarget && !m_pTarget->InFog())
		{
/*
			if (ai()->Frame() - m_lastAttackFrame > Agent()->AvgCoolDown() + 100)
				if (!(m_pTarget->GroundAttack() || m_pTarget->Is(Terran_Bunker)))
				{
					assert_throw(false);
				//	bw << Agent()->NameWithId() << " unsieging (time elapsed)" << endl;
				//	ai()->SetDelay(500);
					return ChangeState(unsieging);
				}
*/
		///	bw << " +  " << ai()->Frame() << endl;
			if (Agent()->CoolDown() > Agent()->PrevCoolDown())
				if (m_pTarget->IsHisUnit())
					m_delayUntilUnsieging +=
						him().IsTerran() ? m_delayUntilUnsieging*2/3 :
						me().Army().KeepTanksAtHome() ? m_delayUntilUnsieging*1/3 : 50;

			m_staySiegedUntil = ai()->Frame() + m_delayUntilUnsieging;
			if (Agent()->CoolDown() <= 1)
			{
			///	bw << Agent()->NameWithId() << " attack! " << m_pTarget->NameWithId() << endl;
				Agent()->Attack(m_pTarget);
				return;
			}
		}
		else
		{
			if (m_pTarget)
			{
				assert_throw(m_pTarget->InFog());

				if (none_of(Checking::Instances().begin(), Checking::Instances().end(), [this](const Checking * pChecker)
									{ return pChecker->Target() == m_pTarget->Unit(); }))
				{
				///	bw << Agent()->NameWithId() << " needs Checking of " << m_pTarget->NameWithId() << endl;

					if (MyUnit * pCandidate = Checking::FindCandidate(m_pTarget))
					{
						m_staySiegedUntil = max(m_staySiegedUntil, ai()->Frame() + 90);
						pCandidate->ChangeBehavior<Checking>(pCandidate, m_pTarget);
					}
				}

				m_pTarget = nullptr;
			}

		///	bw << " -  " << ai()->Frame() << endl;
			if (ai()->Frame() > m_staySiegedUntil)
			{
			///	bw << Agent()->NameWithId() << " unsieging (no target)" << endl;
			///	ai()->SetDelay(500);
				return ChangeState(unsieging);
			}

		///	bw << Agent()->NameWithId() << " unsieging (no target) : waiting " << 100 - (ai()->Frame() - m_lastAttackFrame) << endl;
		///	ai()->SetDelay(50);
		}
	}
}


void Sieging::OnFrame_unsieging()
{CI(this);
	if (JustArrivedInState())
	{
		m_pTarget = nullptr;
		m_inStateSince = ai()->Frame();
		return Agent()->Unsiege();
	}

	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
	{
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
	else
	{
	}
}



} // namespace iron



