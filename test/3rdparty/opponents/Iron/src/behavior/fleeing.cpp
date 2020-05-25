//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "fleeing.h"
#include "chasing.h"
#include "kiting.h"
#include "fighting.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../units/cc.h"
#include "../Iron.h"


namespace { auto & bw = Broodwar; }


namespace iron
{



const TilePosition dimCC = UnitType(Terran_Command_Center).tileSize();

const double pursuer_angle = cos(pi/4);



const Area * findFarArea(const Area * pFrom)
{
	vector<const Area *> Candidates;

	for (int i = 8 ; i >= 4 ; i -= 2)
	{
		Candidates.clear();
		for (const Area & area : ai()->GetMap().Areas())
			if (area.MaxAltitude() > 32*i)
				if (&area != pFrom)
				if (&area != him().GetArea())
				if (&area != me().GetArea())
				if (area.AccessibleFrom(pFrom))
					Candidates.push_back(&area);

		if (Candidates.size() >= 1)
		{
			return random_element(Candidates);
		}
	}

	return pFrom;
}


vector<const FaceOff *> findPursuers(const MyUnit * pAgent)
{
#if DEV
	bw->drawCircleMap(pAgent->Pos(), pAgent->Sight() + 7*32, Colors::Purple);//1
#endif

	vector<const FaceOff *> Pursuers;
	for (const auto & faceOff : pAgent->FaceOffs())
		if (faceOff.HisAttack())
//		if (faceOff.GroundDistance() < pAgent->Sight() + 5*32)
		if (faceOff.GroundDistanceToHitMe() < pAgent->Sight() + 5*32)
		{
			Vect PA = toVect(pAgent->Pos() - faceOff.His()->Pos());
			PA.Normalize();

			Vect Pacc = faceOff.His()->Acceleration();
			Pacc.Normalize();

			if (PA * Pacc > pursuer_angle)
			{
				Pursuers.push_back(&faceOff);
#if DEV
				bw->drawCircleMap(faceOff.His()->Pos(), 18, Colors::Purple);//1
				drawLineMap(faceOff.His()->Pos(), pAgent->Pos(), Colors::Purple);//1
#endif
			}
		}

	return Pursuers;
}


vector<const FaceOff *> findThreats(const MyUnit * pAgent, int maxDistToHisRange, int * pDistanceToHisFire, bool skipAirUnits)
{
	if (pDistanceToHisFire) *pDistanceToHisFire = numeric_limits<int>::max();

	vector<const FaceOff *> Threats;
	for (const auto & faceOff : pAgent->FaceOffs())
		if (faceOff.HisAttack())
		if (faceOff.DistanceToHisRange() < maxDistToHisRange)
		if (!(skipAirUnits && faceOff.His()->Flying()))
		{
			if (faceOff.His()->Is(Terran_Vulture_Spider_Mine) && faceOff.His()->InFog() && faceOff.His()->IsHisUnit()->WatchedOn())
				continue;

			Threats.push_back(&faceOff);
			if (pDistanceToHisFire)
				if (faceOff.DistanceToHisRange() < *pDistanceToHisFire)
					*pDistanceToHisFire = faceOff.DistanceToHisRange();
		}

	return Threats;
}


vector<const FaceOff *> findThreatsButWorkers(const MyUnit * pAgent, int maxDistToHisRange, int * pDistanceToHisFire)
{
	if (pDistanceToHisFire) *pDistanceToHisFire = numeric_limits<int>::max();

	vector<const FaceOff *> Threats;
	for (const auto & faceOff : pAgent->FaceOffs())
		if (!faceOff.His()->Type().isWorker())
		if (faceOff.HisAttack())
		if (faceOff.DistanceToHisRange() < maxDistToHisRange)
		{
			if (faceOff.His()->Is(Terran_Vulture_Spider_Mine) && faceOff.His()->InFog() && faceOff.His()->IsHisUnit()->WatchedOn())
				continue;

			Threats.push_back(&faceOff);
			if (pDistanceToHisFire)
				if (faceOff.DistanceToHisRange() < *pDistanceToHisFire)
					*pDistanceToHisFire = faceOff.DistanceToHisRange();
		}

	return Threats;
}




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fleeing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Fleeing *> Fleeing::m_Instances;

Fleeing::Fleeing(MyUnit * pAgent, frame_t noAlertFrames)
	: Behavior(pAgent, behavior_t::Fleeing),
	m_state(Agent()->Is(Terran_SCV) && (Agent()->GetArea()->MaxAltitude() > 32*8) ? insideArea : anywhere),
	m_noAlertUntil(ai()->Frame() + noAlertFrames)
{
	assert_throw(!pAgent->Is(Terran_Siege_Tank_Siege_Mode));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
//	ai()->SetDelay(100);

	fill(m_minDistToWorkerPursuerRange, m_minDistToWorkerPursuerRange + size_minDistToWorkerPursuerRange, 0);
}


Fleeing::~Fleeing()
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


void Fleeing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();

	m_pDragOutTarget = nullptr;
}


string Fleeing::StateName() const
{CI(this);
	switch(State())
	{
	case dragOut:		return "dragOut";
	case insideArea:	return (GoAwayFromStuckingBuildingUntil() >= ai()->Frame()) ? "goAwayFromStuckingBuilding" : "insideArea";
	case anywhere:		return (GoAwayFromStuckingBuildingUntil() >= ai()->Frame()) ? "goAwayFromStuckingBuilding" : "anywhere";
	default:			return "?";
	}
}


bool Fleeing::CanRepair(const MyBWAPIUnit * pTarget, int) const
{CI(this);
	if (State() == dragOut) return false;

	if (pTarget->Is(Terran_Missile_Turret) || pTarget->Is(Terran_Goliath))
		if (findThreats(Agent(), 3*32, nullptr, bool("skipAirUnits")).empty())
			return true;

	if (pTarget->GetBehavior()->IsSieging())
		for (Fighting * pFighter : Fighting::Instances())
			if (pFighter->ProtectTank() && (pFighter->Where() == pTarget->Pos()))
				return true;

		if (findThreats(Agent(), 3*32, nullptr, bool("skipAirUnits")).empty())
			return true;

	return CanReadAvgPursuers() && (AvgPursuers() < 0.5);
}


Vect Fleeing::CalcultateDivergingVector() const
{CI(this);
	Fleeing * pCloser = nullptr;
	const int maxDist = 32*10;
	int minDist = maxDist;

	for (Fleeing * pFleeing : Instances())
		if (pFleeing != this)
		{
			int d = Agent()->Pos().getApproxDistance(pFleeing->Agent()->Pos());
			if (d < minDist) 
			{
				minDist = d;
				pCloser = pFleeing;
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


Vect Fleeing::CalcultateThreatVector(const vector<const FaceOff *> & Threats) const
{CI(this);
	Vect dir;
	for (const auto * pFaceOff : Threats)
	{
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
			//	bw->drawTextMap(pHisUnit->Pos().x-5, pHisUnit->Pos().y-8, "%c%d", Text::White, pHisUnit->MinesTargetingThis());
				
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
#if DEV && 1 || DISPLAY_SCV_FLEEING
		drawLineMap(pFaceOff->His()->Pos(), pFaceOff->His()->Pos() + toPosition(32*d), Colors::White, crop);//1
#endif
	}

	dir.Normalize();
	return dir;
}


Vect Fleeing::CalcultateIncreasingAltitudeVector() const
{CI(this);
	const auto w = WalkPosition(Agent()->Pos());
	CHECK_POS(w);
	const altitude_t altitude = ai()->GetMap().GetMiniTile(w).Altitude();
	const altitude_t maxAltitude = Agent()->GetArea()->MaxAltitude();

	Vect dir = toVect(Position(ai()->GetVMap().GetIncreasingAltitudeDirection(WalkPosition(Agent()->Pos()))));
	dir.Normalize();
	dir *= (maxAltitude - min(altitude * 2, (int)maxAltitude)) / 2 / (double)maxAltitude;
	return dir;
}


Vect Fleeing::CalcultateAntiAirVector() const
{CI(this);
	Vect dir;

	if (m_flyingPursuers)
	{
		MyBuilding * pNearestTurret = nullptr;
		int minDist = 20*32;
		for (const auto & b : me().Buildings(Terran_Missile_Turret))
			if (b->Completed())
			{
				int d = groundDist(Agent()->Pos(), b->Pos());
				if (d < minDist)
				{
					minDist = d;
					pNearestTurret = b.get();
				}
			}

		if (pNearestTurret)
		{
			dir = toVect(pNearestTurret->Pos() - Agent()->Pos());
			dir.Normalize();
			dir *= 2;
		}
	}

	return dir;
}


static map<int, vector<WalkPosition>> DeltasByRadius;


void Fleeing::AdjustTarget(Position & target, const int walkRadius) const
{CI(this);
	const int kindOfImpasses =
		((Agent()->GetArea()->AccessibleNeighbours().size() == 1) ||
		(Agent()->GetArea() == him().GetArea()) ||
		 (Agent()->GetArea() == me().GetArea()))
			? 1 : 2;

	vector<Area::id> AreasAllowed = {Agent()->GetArea()->Id()};
	
	if ((State() == anywhere) || (Agent()->Life() < Agent()->PrevLife(30)))
		for (const Area * area : Agent()->GetArea()->AccessibleNeighbours())
//			if (area != me().GetArea())
				AreasAllowed.push_back(area->Id());

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

#if DEV && 0 || DISPLAY_SCV_FLEEING
			drawLineMap(Agent()->Pos(), p, Colors::Black);
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


void Fleeing::OnFrame_v()
{CI(this);
#if DEV
	if ((State() == dragOut) && m_pDragOutTarget) drawLineMap(Agent()->Pos(), m_pDragOutTarget->Pos(), GetColor());
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if ((Agent()->GroundRange() > 3*32) || Agent()->Flying())
	{
		return Agent()->ChangeBehavior<Kiting>(Agent());
	}


	
	{
		int minDistToWorkerPursuerRange = numeric_limits<int>::max();
		m_onlyWorkerPursuers = true;
		m_flyingPursuers = false;
		++m_numberOf_pursuersCount;
		const vector<const FaceOff *> Pursuers = findPursuers(Agent());
		for (const FaceOff * faceOff : Pursuers)
			if (HisUnit * pHisUnit = faceOff->His()->IsHisUnit())
			{
				if (pHisUnit->Flying()) m_flyingPursuers = true;

				pHisUnit->SetPursuingTarget(Agent());
				if (pHisUnit->Type().isWorker())
					minDistToWorkerPursuerRange = min(minDistToWorkerPursuerRange, faceOff->DistanceToHisRange());
				else
					m_onlyWorkerPursuers = false;
			}
		
		if (CanReadAvgPursuers() && (AvgPursuers() >= 1.5)) Alert();

		m_cumulativePursuers += Pursuers.size();

#if DEV
		bw->drawTextMap(Agent()->Pos().x + Agent()->Type().width() + 3, Agent()->Pos().y - Agent()->Type().height() - 10, "%cpursuers: %.1f", Text::Purple, AvgPursuers());//1
#endif
		if (m_numberOf_pursuersCount > 20)
		{
			m_avgPursuers = m_cumulativePursuers / double(m_numberOf_pursuersCount);
			m_cumulativePursuers = 0;
			m_numberOf_pursuersCount = 0;
			m_canReadAvgPursuers = true;
		}

		if (m_onlyWorkerPursuers)
		{
			for (int i = size_minDistToWorkerPursuerRange-1 ; i > 0 ; --i)
				m_minDistToWorkerPursuerRange[i] = m_minDistToWorkerPursuerRange[i-1];
			m_minDistToWorkerPursuerRange[0] = minDistToWorkerPursuerRange;
/*
			bw << m_minDistToWorkerPursuerRange[0] - m_minDistToWorkerPursuerRange[1] << "   ";
			bw << m_minDistToWorkerPursuerRange[0] - m_minDistToWorkerPursuerRange[2] << "   ";
			bw << m_minDistToWorkerPursuerRange[0] - m_minDistToWorkerPursuerRange[3] << "   ";
			bw << endl;
*/
		}

	}

	switch (State())
	{
	case dragOut:		OnFrame_dragOut(); break;
	case insideArea:
	case anywhere:		OnFrame_anywhere(); break;
	default: assert_throw(false);
	}
}




bool Fleeing::AttackedAndSurrounded() const
{CI(this);
	assert_throw(Agent()->IsMy<Terran_SCV>());

	if (Agent()->Life() < Agent()->PrevLife(5))
	{
		if (him().StartingBase() && (him().StartingBase()->Center().getApproxDistance(Agent()->Pos()) < 5*32))
		{
			if (Agent()->PrevLife(13) - Agent()->Life() >= 10)
				return true;
		
			if (findThreats(Agent(), 1*32).size() >= 2)
				return true;
		}
		else
		{
			if (Agent()->PrevLife(20) - Agent()->Life() >= 15)
				return true;
		
			if (findThreats(Agent(), 40).size() >= 3)
				return true;
		}
	}

	return false;
}


void Fleeing::Alert()
{CI(this);/*
	if (ai()->Frame() < m_noAlertUntil) return;

	const int alertRadius = 32*((m_onlyWorkerPursuers ? 2 : 5) + (CanReadAvgPursuers() ? int(AvgPursuers()+0.5) : 1));//1
#if DEV
	bw->drawCircleMap(Agent()->Pos(), alertRadius, GetColor());//1
#endif
	for (MyUnit * u : me().Units()) if (u != Agent())
		if (u->Pos().getApproxDistance(Agent()->Pos()) < alertRadius)
		{
			u->OnDangerHere();
#if DEV
			bw->drawCircleMap(u->Pos(), 10, GetColor());//1
			drawLineMap(Agent()->Pos(), u->Pos(), GetColor());//1
#endif
		}
*/
}


void Fleeing::OnMineralDestroyed(Mineral * m)
{CI(this);
	if (m == m_pDragOutTarget)
		m_pDragOutTarget = nullptr;
}


void Fleeing::OnFrame_dragOut()
{CI(this);
	if (JustArrivedInState())
	{
		assert_throw(Agent()->IsMy<Terran_SCV>());
		m_inStateSince = ai()->Frame();
//		if (!him().StartingBase() || (him().StartingBase()->Center().getApproxDistance(Agent()->Pos()) > 8*32))
//			Alert();

		m_pDragOutTarget = nullptr;
		int maxDist = numeric_limits<int>::min();
		for (const auto * base : me().Bases())
			for (Mineral * m : base->BWEMPart()->Minerals())
				if (m->Unit()->isVisible())
					if (!m_pDragOutTarget || (roundedDist(m->Pos(), Agent()->Pos()) > maxDist))
					{
						m_pDragOutTarget = m;
						maxDist = roundedDist(m->Pos(), Agent()->Pos());
					}
	
		if (m_pDragOutTarget)
			return Agent()->IsMy<Terran_SCV>()->Gather(m_pDragOutTarget);

		return Agent()->Move(Position(me().GetArea()->Top()));
	}

	if (ai()->Frame() - m_inStateSince > 50)
		if (findThreats(Agent(), 2*32).size() < 2)
			return ChangeState(insideArea);
}


void Fleeing::OnFrame_anywhere()	// also insideArea
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		m_lastPos = Positions::None;
	}

	if (GoAwayFromStuckingBuildingUntil() >= ai()->Frame())
		return;
	else if (ai()->Frame() - m_inStateSince > 15)
		if (CheckStuckBuilding())
			return;

	if (My<Terran_SCV> * pSCV = Agent()->IsMy<Terran_SCV>())
	{
		bool flyingThreats = false;
		for (const auto & faceOff : Agent()->FaceOffs())
			if (faceOff.His()->Flying())
			if (faceOff.HisAttack())
			if (faceOff.DistanceToHisRange() < 5*32)
			{
				flyingThreats = true;
				break;
			}

	//	if (pSCV->GetArea() != me().GetArea())
		if (!flyingThreats)
		{
			if (AttackedAndSurrounded())
				return ChangeState(dragOut);

			if (Agent()->Life() < Agent()->PrevLife(5))
				if (ai()->Frame() - Agent()->LastFrameMoving() > 15)
					return ChangeState(dragOut);
		}

		if (ai()->Frame() - m_inStateSince > 50)
			if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
				return ChangeState(dragOut);
	}


	int distanceToHisFire;
	vector<const FaceOff *> Threats = findThreats(Agent(), 4*32, &distanceToHisFire);

	if (State() == insideArea)
	{
		if ((distanceToHisFire < 2*32) || !m_onlyWorkerPursuers || (Agent()->Life() <= 10))
		{
			m_remainingFramesInFreeState = 80;
			return ChangeState(anywhere);
		}
		else m_remainingFramesInFreeState = 1000000;
	}
	else
	{
		assert_(State() == anywhere);
		if (--m_remainingFramesInFreeState == 0) return ChangeState(insideArea);
	}


	if ((m_numberOf_pursuersCount == 0) && CanReadAvgPursuers())
	{
		if (AvgPursuers() > 2.5)
		{
			if (State() == insideArea)
				return ChangeState(anywhere);
		}
		else if (AvgPursuers() < 0.5)
		{
			if (Agent()->Life() == Agent()->PrevLife(30))
			{
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
			}
		}
		else if (AvgPursuers() < 1.1)
		{
			auto Threats = findThreats(Agent(), 4*32);
			if (Threats.size() == 1)
				if (HisUnit * pHisUnit = Threats.front()->His()->IsHisUnit())
					if (!pHisUnit->InFog())
						if (pHisUnit->Type().isWorker() || pHisUnit->Is(Terran_Marine) || pHisUnit->Is(Zerg_Zergling))
							if (pHisUnit->PursuingTarget() == Agent())
								if (pHisUnit->MinesTargetingThis().empty())
									if (!(ai()->GetStrategy()->Has<ZerglingRush>() && !ai()->GetStrategy()->Detected<ZerglingRush>() && !Agent()->GetStronghold()))
									{
										if (Threats.front()->FramesToKillMe() > Threats.front()->FramesToKillHim() + 10)
										{
										///	bw << "hé ! " << Agent()->NameWithId() << endl; ai()->SetDelay(3000);
											return Agent()->ChangeBehavior<Chasing>(Agent(), pHisUnit);
										}
										else //if (pHisUnit->Chasers().empty())
										{
											if (Chasing * pChaser = Chasing::GetChaser(pHisUnit))
											{
											///	if (Threats.front()->His()->Is(Zerg_Zergling))
											///	{bw << pChaser->Agent()->NameWithId() << "helps " << Agent()->NameWithId() << endl; ai()->SetDelay(10000);}
												//if (pHisUnit->Chasers().size() < 2)
												if ((Agent()->Life() >= 40) ||
													((Agent()->Life() >= 20) && (Threats.front()->FramesToKillMe() > Threats.front()->FramesToKillHim() - 30)))
												{
													return Agent()->ChangeBehavior<Chasing>(Agent(), pHisUnit);
												}
											}

										}/*
										else if (pHisUnit->Chasers().size() < 2)
										{
											return Agent()->ChangeBehavior<Chasing>(Agent(), pHisUnit);
										}*/
									}
		}
	}


	if (!Threats.empty() && (!m_onlyWorkerPursuers || (distanceToHisFire < 4*32)))
	{
		Vect threatForce = CalcultateThreatVector(Threats);
		Vect divergingForce = CalcultateDivergingVector();
		Vect altitudeForce = CalcultateIncreasingAltitudeVector();
		Vect antiAirForce = CalcultateAntiAirVector();
		const double altitudeCoef = min(4*32, max(0, distanceToHisFire)) / double(4*32);
		Vect dir = threatForce + divergingForce + altitudeForce*altitudeCoef + antiAirForce;
		dir.Normalize();

		const int buildingsAndNeutralsAround = ai()->GetVMap().BuildingsAndNeutralsAround(TilePosition(Agent()->Pos()), 3);
		const int walkRadius =	(buildingsAndNeutralsAround == 0) ? 24 :
								(buildingsAndNeutralsAround == 1) ? 12 : 8;

		Position delta = toPosition(dir * (8*walkRadius));
		Position target = Agent()->Pos() + delta;
		AdjustTarget(target, walkRadius);
		if (ai()->GetMap().Valid(target))
		{
			m_lastPos = Agent()->Pos();
			m_lastTarget = target;
#if DEV && 0 || DISPLAY_SCV_FLEEING
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(threatForce*8*walkRadius), Colors::Red, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(divergingForce*8*walkRadius), Colors::Blue, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(altitudeForce*8*walkRadius), Colors::Yellow, crop);//1
			drawLineMap(Agent()->Pos(), Agent()->Pos() + toPosition(antiAirForce*8*walkRadius), Colors::Orange, crop);//1
			drawLineMap(Agent()->Pos(), target, GetColor());//1
#endif
			return Agent()->Move(target);
		}
	}
	else
	{
		if (ai()->Frame() - Agent()->LastFrameMoving() > 1)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

		return Agent()->Stop();
	}

}



} // namespace iron



