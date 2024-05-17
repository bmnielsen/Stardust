//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "chasing.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/dragoonRush.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/workerRush.h"
#include "../units/cc.h"
#include "../vect.h"
#include "../Iron.h"


namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Chasing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Chasing *> Chasing::m_Instances;

Chasing * Chasing::GetChaser(HisUnit * pTarget, int maxDist)
{
	CI(pTarget);

	int minDist = numeric_limits<int>::max();
	My<Terran_SCV> * pBestCandidate = nullptr;

	for (const auto & u : me().Units(Terran_SCV))
		if (u->Completed() && !u->Loaded())
			if ((u->Life() >= 40) || ((u->Life() >= 20) && (u->Life() >= pTarget->LifeWithShields())))
			{
				int length;
				ai()->GetMap().GetPath(pTarget->Pos(), u->Pos(), &length);
				if ((length >= 0) && (length < minDist))
				{
					if (u->GetBehavior()->CanChase(pTarget))
					{
						minDist = length;
						pBestCandidate = u->As<Terran_SCV>();
					}
				}
			}

	if (!pBestCandidate) return nullptr;
	if (minDist > maxDist) return nullptr;
	
	pBestCandidate->ChangeBehavior<Chasing>(pBestCandidate, pTarget, bool("insist"));
	return pBestCandidate->GetBehavior()->IsChasing();
}


Chasing::Chasing(MyUnit * pAgent, BWAPIUnit * pTarget, bool insist, frame_t maxChasingTime, bool workerDefense)
	: Behavior(pAgent, behavior_t::Chasing), m_pTarget(pTarget), m_FaceOff(pAgent, CI(pTarget)->IsHisBWAPIUnit()),
	m_insist(insist), m_maxChasingTime(maxChasingTime), m_workerDefense(workerDefense)
{
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	assert_throw(pTarget);

	m_pTarget->AddChaser(this);

	m_chasingSince = ai()->Frame();
	m_lastFrameTouchedHim = ai()->Frame();

//	if (pTarget->Is(Zerg_Zergling))
//	{ bw << pAgent->NameWithId() << " chasing " << pTarget->NameWithId() << endl; ai()->SetDelay(500); }
}


Chasing::~Chasing()
{
#if !DEV
	try //3
#endif
	{
		CI(m_pTarget)->RemoveChaser(this);

		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Chasing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Chasing::StateName() const
{CI(this);
	switch(State())
	{
	case chasing:			return m_insist ? "chasing !" : "chasing";
	default:				return "?";
	}
}


static void adjustDestWithAltitude(Position & dest)
{
	WalkPosition bestW = WalkPosition(dest);
	altitude_t maxAltitude = -1;
	for (int dy = -1 ; dy <= +1 ; ++dy)
	for (int dx = -1 ; dx <= +1 ; ++dx)
	{
		WalkPosition w (WalkPosition(dest) + WalkPosition(dx, dy));
		if (ai()->GetMap().Valid(w))
		{
			altitude_t altitude = ai()->GetMap().GetMiniTile(w, check_t::no_check).Altitude();
			if (altitude > maxAltitude)
			{
				maxAltitude = altitude;
				bestW = w;
			}
		}
	}

	dest = center(bestW);
}


static void adjustDestWithBlockedTiles(Position origin, Position & dest)
{
	const int margin = 5;
	const TilePosition originTile(origin);

	const bool vertical = abs(dest.y - origin.y) > abs(dest.x - origin.x);

	bool adjusted = false;
	if ((dest.y - origin.y < margin) && !canWalkOnTile(originTile + TilePosition(0, -1)))	{ dest.y = origin.y + margin; adjusted = true; }
	if ((dest.y - origin.y > margin) && !canWalkOnTile(originTile + TilePosition(0, 1)))		{ dest.y = origin.y - margin; adjusted = true; }
	if ((dest.x - origin.x < margin) && !canWalkOnTile(originTile + TilePosition(-1, 0)))	{ dest.x = origin.x + margin; adjusted = true; }
	if ((dest.x - origin.x > margin) && !canWalkOnTile(originTile + TilePosition(1, 0)))		{ dest.x = origin.x - margin; adjusted = true; }

	if (!adjusted)
	{
		if ((dest.x - origin.x < margin) && (dest.y - origin.y < margin) && !canWalkOnTile(originTile + TilePosition(-1, -1)))	
			if (vertical) dest.x = origin.x + margin;
			else          dest.y = origin.y + margin;

		if ((dest.x - origin.x > margin) && (dest.y - origin.y < margin) && !canWalkOnTile(originTile + TilePosition(1, -1)))	
			if (vertical) dest.x = origin.x - margin;
			else          dest.y = origin.y + margin;

		if ((dest.x - origin.x < margin) && (dest.y - origin.y > margin) && !canWalkOnTile(originTile + TilePosition(-1, 1)))	
			if (vertical) dest.x = origin.x + margin;
			else          dest.y = origin.y - margin;

		if ((dest.x - origin.x > margin) && (dest.y - origin.y > margin) && !canWalkOnTile(originTile + TilePosition(1, 1)))	
			if (vertical) dest.x = origin.x - margin;
			else          dest.y = origin.y - margin;
	}
}


void Chasing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (Target() == other)
	{
		assert_throw(!Agent()->Is(Terran_Siege_Tank_Siege_Mode));
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}


void Chasing::OnFrame_v()
{CI(this);
	CI(Target());

#if DEV || DISPLAY_SCV_CHASING
	drawLineMap(Agent()->Pos(), Target()->Pos(), GetColor());//1
	bw->drawBoxMap(Target()->Pos() - 25, Target()->Pos() + 25, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	m_FaceOff = FaceOff(Agent()->IsMyUnit(), Target()->IsHisBWAPIUnit());


//	const frame_t noAlertTimeIfGoFleeing = InGroup() ? 50 : 0;
	const frame_t noAlertTimeIfGoFleeing = 0;

	frame_t maxChasingTime = 150;
	if (Agent()->GetArea() == him().GetArea()) maxChasingTime = 300;
	else if (him().StartingBase() && contains(him().GetArea()->AccessibleNeighbours(), Agent()->GetArea())) maxChasingTime = 200;
	else if (Agent()->GetArea() == me().GetArea()) maxChasingTime = 300;

	if (ai()->GetStrategy()->Detected<DragoonRush>())
		if (Target()->Is(Protoss_Dragoon))
			maxChasingTime = 500;

	if (ai()->GetStrategy()->Detected<WorkerRush>())
		if (!m_workerDefense)
			maxChasingTime = 300;

	if (m_maxChasingTime) maxChasingTime = m_maxChasingTime;

	if (ai()->Frame() - m_lastFrameTouchedHim > maxChasingTime)
	{
	///	bw << "!!!  " << Agent()->NameWithId() << " " << ai()->Frame() - m_lastFrameTouchedHim << " > " << maxChasingTime << endl; ai()->SetDelay(5000);
		Agent()->IsMyUnit()->SetNoChaseFor(30);
		assert_throw(!Agent()->Is(Terran_Siege_Tank_Siege_Mode));
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
	
	if (auto * u = Target()->IsHisUnit())
		if (!u->MinesTargetingThis().empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit());

	bool mayFlee = true;
/*
	if (ai()->GetStrategy()->Detected<DragoonRush>())
		if (Target()->Is(Protoss_Dragoon))
			mayFlee = false;
*/
	if (Target()->GroundRange() > 3*32)
		if (Target()->AvgCoolDown() <= 30)
			if (Agent()->PrevDamage() > 5)
				if (Agent()->Life() < Agent()->PrevLife(18)) 
					if (Agent()->Life() <= 2*Agent()->PrevDamage())
//						if (GetFaceOff().AirDistance() < 40)
						if (GetFaceOff().AirDistanceToHitMe() < 40)
						{
							mayFlee = false;
						//	bw << "! mayFlee  " << Agent()->NameWithId() << endl; ai()->SetDelay(5000);
						}
					
	if (mayFlee)
		if (Target()->AvgCoolDown() >= 4)
			if (Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() - 3)) - Agent()->Life() > GetFaceOff().HisAttack())	// means damage from at least two threats
//			if (!Target()->Is(Terran_Marine) && Agent()->Unit()->isMoving())
				if (!Target()->Is(Terran_Marine))
					if (Target()->FramesToBeKilledByChasers() > GetFaceOff().FramesToKillMe() / 2 - 10)	// flee only if Target is far from being killed
					{
					///	bw << "hey <other threat> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000);
					///	bw << Target()->AvgCoolDown() + 3 << " " << Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() + 3)) << " - " << Agent()->Life() << " = " << Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() - 3)) - Agent()->Life() << " > " << GetFaceOff().HisAttack() << endl;
						return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), 50);
					}

	if (mayFlee)
		if (GetFaceOff().FramesToKillMe() < Target()->FramesToBeKilledByChasers() + 2*(int)Target()->Chasers().size())
			if (m_insist || Target()->Is(Terran_Marine))
			{
			//	if (!m_insist) { bw << "!! " << Agent()->NameWithId() << endl; if (ai()->Delay() == 0) ai()->SetDelay(5000); }

				if (Agent()->Life() <= (2 + (Target()->Is(Terran_Marine) ? 2 : 0))*GetFaceOff().HisAttack() + 1)
				{
				///	bw << "hey <loosing (insist/marine)> !  " << Agent()->NameWithId() << "  FramesToKillMe = " << GetFaceOff().FramesToKillMe() << "  FramesToBeKilledByChasers = " << Target()->FramesToBeKilledByChasers() << endl;  ai()->SetDelay(2000);
					return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), 50);
				}
			}
			else
			{
				if ((Agent()->Life() <= (2 + (Target()->Is(Terran_Marine) ? 1 : 0))*GetFaceOff().HisAttack() + 1) ||
					!m_touchedHim || (ai()->Frame() - m_lastFrameTouchedHim > 20) || Agent()->CoolDown()	)
				{
				//	if (Agent()->Life() <= (2 + (Target()->Is(Terran_Marine) ? 2 : 0))*GetFaceOff().HisAttack() + 1)
				//	{bw << "hey <loosing> !  " << Agent()->NameWithId() << "  FramesToKillMe = " << GetFaceOff().FramesToKillMe() << "  FramesToBeKilledByChasers = " << Target()->FramesToBeKilledByChasers() << endl; ai()->SetDelay(5000);}

					return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), 50);
				}
			}


	if (mayFlee)
	{
		// Cf AttackedAndSurrounded (TODO: refactor)
/*
		if (Agent()->Life() + 8 < Agent()->PrevLife(24))
			if (GetFaceOff().FramesToKillMe()/2 < Target()->FramesToBeKilledByChasers() + 2*(int)Target()->Chasers().size())
			{
				bw << "hey 1!  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000);
				return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), noAlertTimeIfGoFleeing);
			}
*/
		if (Agent()->Life() < Agent()->PrevLife(20))
			if (!(ai()->GetStrategy()->Detected<ZerglingRush>() && (groundDist(Agent()->Pos(), me().StartingBase()->Center()) < 10*32)))
			{
				auto Threats1 = findThreats(Agent()->IsMyUnit(), 1*32);
				int countThreats1 = Threats1.size();
				if (countThreats1 >= 2)
				{
				//	bw << "hey <surrounded> !  " << Agent()->NameWithId() << " " << Agent()->Life() << " " << countThreats1 << " " << GetFaceOff().FramesToKillMe() / countThreats1 << " " << GetFaceOff().FramesToKillMe() / countThreats1 - 10 << " compared to " << Target()->FramesToBeKilledByChasers() << endl; //ai()->SetDelay(2000);
					if (Target()->FramesToBeKilledByChasers() > GetFaceOff().FramesToKillMe() / countThreats1 - 10)	// flee only if Target is far from being killed
					{
					//	bw << "--> hey <surrounded> !  " << Agent()->NameWithId() << endl; //ai()->SetDelay(2000);
						return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), noAlertTimeIfGoFleeing);
					}
				}
			}
	}

	if (mayFlee)
	{
		auto Threats3 = findThreats(Agent()->IsMyUnit(), 3*32);
		for (auto * faceOff : Threats3)
			if (faceOff->His()->IsHisBuilding() || faceOff->His()->Is(Terran_Siege_Tank_Siege_Mode))
			{
			///	if (faceOff->His()->IsHisBuilding()) { bw << "--> stop chasing <dangerous building> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000); }
			///	if (faceOff->His()->Is(Terran_Siege_Tank_Siege_Mode)) { bw << "--> stop chasing <tank> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000); }
				return Agent()->ChangeBehavior<Fleeing>(Agent()->IsMyUnit(), noAlertTimeIfGoFleeing);
			}
	}

	switch (State())
	{
	case chasing:		OnFrame_chasing(); break;
	default: assert_throw(false);
	}
}


bool Chasing::CanRepair(const MyBWAPIUnit * , int) const
{CI(this);
	if (ai()->Frame() - m_lastFrameTouchedHim > 300)
		if (Target()->LifeWithShields() > 20)
			return true;


//	if (GetFaceOff().FramesToKillHim() < 80) return false;
//	if (Target()->Life() < Target()->MaxLife()) return false;

//	return true;
	return false;
}


void Chasing::OnFrame_chasing()
{CI(this);
	if (JustArrivedInState())
	{
		m_chasingSince = ai()->Frame();
		m_lastFrameTouchedHim = ai()->Frame();
		m_firstFrameTouchedHim = 0;
		m_touchedHim = false;
///		bw << Agent()->NameWithId() << "chases " << Target()->NameWithId() << endl; ai()->SetDelay(5000);
	}

	if (Agent()->Pos() == Agent()->PrevPos(5) && Agent()->Pos() == Agent()->PrevPos(10))
	if (ai()->Frame() - m_lastFrameTouchedHim > 20)
	{
		if (ai()->Frame() - m_lastFrameMovedToRandom > 10)
		{
		///	bw << Agent()->NameWithId() << " blocked: move to random position" << endl; ai()->SetDelay(500);
			m_lastFrameMovedToRandom = ai()->Frame();
			return Agent()->Move(ai()->GetVMap().RandomPosition(Agent()->Pos(), 5*32));
		}
		else return;
	}

	if (Target()->Type().isWorker())
		if (Target()->Chasers().size() >= 2)
		{
			int targetDistToMinerals = numeric_limits<int>::max();
			for (Mineral * m : Target()->GetArea()->Minerals())
			{
				int d = distToRectangle(Target()->Pos(), m->TopLeft(), m->Size());
				if (d < targetDistToMinerals)
					targetDistToMinerals = d;
			}

			if (m_touchedHim)
			{
				auto Chasers = Target()->Chasers();		// local copy because of deletion/iteration conflict
				for (Chasing * pChaser : Chasers)
				{
					pChaser->m_noMoreChasers = true;
					if (pChaser != this)
					if (pChaser->m_chasingSince == m_chasingSince)
						if (((targetDistToMinerals < 4*32) &&
								((Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()) > 4*32) ||
								(!pChaser->m_firstFrameTouchedHim && m_firstFrameTouchedHim &&
									(
										((ai()->Frame() - m_firstFrameTouchedHim > 50) && (Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()) < 2*32))
									||
										((ai()->Frame() - m_firstFrameTouchedHim > 40) && (Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()) >= 2*32))
									)
								))) ||
							((targetDistToMinerals >= 4*32) &&
								((!pChaser->m_firstFrameTouchedHim && m_firstFrameTouchedHim &&
									(
										((ai()->Frame() - m_firstFrameTouchedHim > 80) && (Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()) < 2*32))
									||
										((ai()->Frame() - m_firstFrameTouchedHim > 60) && (Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()) >= 2*32))
									)
								)))
							)
						{
						//	bw << pChaser->Agent()->NameWithId() << " leaves the group attack" << endl; ai()->SetDelay(500);
							pChaser->Agent()->ChangeBehavior<Fleeing>(pChaser->Agent()->IsMyUnit(), 50);
						}
				}
			}
			else
			{
				if (targetDistToMinerals < 4*32)
				{
					multimap<int, Chasing *> ChasersByDistToTarget;
					for (Chasing * pChaser : Target()->Chasers())
						ChasersByDistToTarget.emplace(Target()->Pos().getApproxDistance(pChaser->Agent()->Pos()), pChaser);

					if (this == ChasersByDistToTarget.begin()->second)	// this Chasing is the closest to the target.
						if (ChasersByDistToTarget.begin()->first > 2*32)
							if (Agent()->Pos().getApproxDistance((++ChasersByDistToTarget.begin())->second->Agent()->Pos()) > 48)
								if ((++ChasersByDistToTarget.begin())->first - ChasersByDistToTarget.begin()->first > 2*32)
								{
								//	ai()->SetDelay(500);
								//	bw << Agent()->NameWithId() << " is waiting for the second chaser" << endl;
									return Agent()->Move(Agent()->Pos());
								}
				}
			}
		}

	Vect Aacc(Agent()->Acceleration());
	Vect Tacc(Target()->Acceleration());
	Vect AT = toVect(Target()->Pos() - Agent()->Pos());

	double normAacc = Aacc.Norm();
	double normTacc = Tacc.Norm();
	double normAT = AT.Norm();

	if (!Agent()->CoolDown())
	{
		if ( (normAT < 32) ||
			((normAT < 80) && ((normTacc < 1.5) ||
							  ((normAacc > 0.1) && ((Aacc * Tacc) / (normAacc * normTacc) < -0.7)))))
		{
///			bw << ai()->Frame() << " : " << Agent()->NameWithId() << " " << normAT << endl;
		///	bw << normAT << endl;
#if DEV
			bw->drawCircleMap(Target()->Pos(), 15, Colors::Yellow);//1
#endif
			if (ai()->Frame() - m_lastFrameAttack > 5)
			{
				m_lastFrameAttack = ai()->Frame();
///				bw << ai()->Frame() << " : " << Agent()->NameWithId() << " attack!" << endl; //ai()->SetDelay(500);
				Agent()->Attack(Target());
			}
			return;
		}
	}
	else
	{
		m_touchedHim = true;
		m_lastFrameTouchedHim = ai()->Frame();
		if (!m_firstFrameTouchedHim) m_firstFrameTouchedHim = ai()->Frame();
//		if (normTacc < 0.1) return;
		if (normAT < 45) return;

	///	bw << ai()->Frame() << " : " << Agent()->NameWithId() << " !!!" << endl; ai()->SetDelay(5000);
	///	bw << normAT << "!!!" << endl;
	}

	if (m_touchedHim)
		if (Agent()->Life() < Agent()->PrevLife(25)) return;		// keep attacking (not moving) if enemy is kiting.

	Vect uAT = AT; uAT.Normalize();
	Vect uDir = uAT;
	if (normTacc > 0.1)
	{
		Vect uTacc = Tacc; uTacc.Normalize();

#if DEV || DISPLAY_SCV_CHASING
		drawLineMap(Target()->Pos(), Target()->Pos() + toPosition(uTacc*75), Colors::Red, crop);//1
#endif

		double angle_TA_Tacc = angle(-uAT, uTacc);
	
		if (abs(angle_TA_Tacc) < pi/4)
		{
			uDir = rot(uAT, -angle_TA_Tacc);
		}
		else
		{
			uDir = uAT*normAacc + uTacc*normTacc;
			uDir.Normalize();
		}
	}
	
	Position delta = toPosition(uDir * 75);
	Position dest = ai()->GetMap().Crop(Agent()->Pos() + delta);

#if DEV || DISPLAY_SCV_CHASING
	drawLineMap(Agent()->Pos(), dest, Colors::Grey, crop);//1
#endif

	if (normAT > 3*32)
		for (int i = 0 ; (i < 3) && (dist(Agent()->Pos(), dest) > 64) ; ++i)
			adjustDestWithAltitude(dest);

	CHECK_POS(TilePosition(dest));
	adjustDestWithBlockedTiles(Agent()->Pos(), dest);
	CHECK_POS(TilePosition(dest));

#if DEV || DISPLAY_SCV_CHASING
	drawLineMap(Agent()->Pos(), dest, Colors::Green);//1
#endif

	if (queenWiseDist(Agent()->Pos(), dest) < 32)
	{
	//	{bw << "no bypassing !  " << Agent()->NameWithId() << endl; if (!ai()->Delay()) ai()->SetDelay(50);}
	//	return ChangeState(bypassing);
		Agent()->Attack(Target());
		return;
	}

	if (ai()->GetMap().GetArea(WalkPosition(dest)) != Target()->GetArea())
		dest = Target()->Pos();

	Agent()->Move(dest);
///	bw << Agent()->NameWithId() << "move " << dest << " !!!!!!!!!!!!!" << endl;
}


} // namespace iron



