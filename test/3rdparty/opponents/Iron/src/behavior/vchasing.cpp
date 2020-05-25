//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "vchasing.h"
#include "fleeing.h"
#include "kiting.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/walling.h"
#include "../vect.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

static bool medicsCouldHelpTarget(const MyUnit * u, const HisUnit * pTarget, int maxDistToTarget)
{
	if (pTarget->Type().isOrganic())
		for (const auto & faceOff : u->FaceOffs())
			if (faceOff.His()->Is(Terran_Medic))
				if (squaredDist(pTarget->Pos(), faceOff.His()->Pos()) < maxDistToTarget * maxDistToTarget)
					return true;

	return false;
}


HisUnit * findVChasingAloneTarget(MyUnit * u)
{
	if (him().IsTerran())
		if (u->Is(Terran_Vulture) ||
			u->Is(Terran_Siege_Tank_Tank_Mode) ||
			u->Is(Terran_Goliath))
		return nullptr;

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (!u->Flying())
			return nullptr;

	if (u->Is(Terran_Goliath)) return nullptr;

	int distanceToHisFire;
	auto Threats = findThreats(u, 5*32, &distanceToHisFire);

	if (Threats.size() == 1)
		if (distanceToHisFire < 3*32)
		{
			const auto * faceOff = Threats.front();
			if (faceOff->MyAttack())
				if (HisUnit * pHisUnit = faceOff->His()->IsHisUnit())
//					if (pHisUnit->Unit()->isVisible())
					if (!pHisUnit->InFog())
						if (pHisUnit->MinesTargetingThis().empty())
							if (faceOff->FramesToKillMe() > faceOff->FramesToKillHim() + 5 /*10 + u->AvgCoolDown()*/)
								if (pHisUnit->Is(Terran_Ghost) ||
									pHisUnit->Is(Terran_Marine) && !u->Flying() ||
									pHisUnit->Is(Terran_Goliath) && (!u->Flying() || (pHisUnit->Life() <= 14 && faceOff->FramesToKillMe() > 40 + faceOff->FramesToKillHim())) ||
									pHisUnit->Is(Terran_Vulture) ||
									pHisUnit->Is(Terran_Siege_Tank_Tank_Mode) || 
									pHisUnit->Is(Terran_Siege_Tank_Siege_Mode) || 

									pHisUnit->Is(Protoss_Dragoon) && !u->Flying() || 
									pHisUnit->Is(Protoss_Reaver) || 

									pHisUnit->Is(Zerg_Hydralisk) && !u->Flying() ||
									pHisUnit->Is(Zerg_Zergling) && u->Is(Terran_Marine) && (ai()->GetStrategy()->Detected<ZerglingRush>() || him().ZerglingPressure()))
								{
									if (!medicsCouldHelpTarget(u, pHisUnit, 4*32))	// medic's range is 2*32
										return pHisUnit;
								}
		}

	return nullptr;
}


HisUnit * findVChasingHelpSCVs(MyUnit * pMyUnit)
{
	int minDist = 50*32;
	HisUnit * pTarget = nullptr;
	for (const auto & u : him().Units())
		if (!u->Chasers().empty())
		{
			int d = groundDist(u.get(), pMyUnit);
			if (d < minDist)
			{
				minDist = d;
				pTarget = u.get();
			}
		}

	return pTarget;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VChasing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<VChasing *> VChasing::m_Instances;


VChasing::VChasing(MyUnit * pAgent, BWAPIUnit * pTarget, bool dontFlee)
	: Behavior(pAgent, behavior_t::VChasing), m_pTarget(pTarget), m_FaceOff(pAgent, pTarget->IsHisBWAPIUnit()), m_dontFlee(dontFlee)
{
	assert_throw(pTarget);
	//assert_throw(pAgent->Is(Terran_Vulture));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_pTarget->AddVChaser(this);

	m_chasingSince = ai()->Frame();
	m_lastFrameTouchedHim = ai()->Frame();

//	if (pTarget->Is(Zerg_Zergling))
//	{ bw << pAgent->NameWithId() << " chasing " << pTarget->NameWithId() << endl; ai()->SetDelay(500); }
}


VChasing::~VChasing()
{
#if !DEV
	try //3
#endif
	{
		m_pTarget->RemoveVChaser(this);

		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void VChasing::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string VChasing::StateName() const
{CI(this);
	switch(State())
	{
	case chasing:			return "chasing";
	default:				return "?";
	}
}


void VChasing::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (Target() == other)
	{
		assert_throw(!Agent()->Is(Terran_Siege_Tank_Siege_Mode));
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}


void VChasing::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), Target()->Pos(), GetColor());//1
	bw->drawBoxMap(Target()->Pos() - 25, Target()->Pos() + 25, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	m_FaceOff = FaceOff(Agent()->IsMyUnit(), Target()->IsHisBWAPIUnit());
/* TODO g
	if (Agent()->GroundRange() > 3*32)
		if (m_FaceOff.DistanceToHisRange() < 2*32)
			return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
*/
	if (auto * u = Target()->IsHisUnit())
		if (!u->MinesTargetingThis().empty())
			return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());

	if (!m_dontFlee)
	{
		if (Target()->AvgCoolDown() >= 4)
			if (Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() - 3)) - Agent()->Life() > GetFaceOff().HisAttack())	// means another threat has come
	//			if (!Target()->Is(Terran_Marine) && Agent()->Unit()->isMoving())
	//			if (!Target()->Is(Terran_Marine))
	//				if (Target()->FramesToBeKilledByVChasers() > GetFaceOff().FramesToKillMe() / 2 - 10)	// flee only if Target is far from being killed
					{
					///	bw << "hey <other threat> !  " << Agent()->NameWithId() << endl; //ai()->SetDelay(2000);
					///	bw << Target()->AvgCoolDown() + 3 << " " << Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() + 3)) << " - " << Agent()->Life() << " = " << Agent()->PrevLife(min(BWAPIUnit::sizePrevLife-1, Target()->AvgCoolDown() - 3)) - Agent()->Life() << " > " << GetFaceOff().HisAttack() << endl;
						return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
					}

		if (GetFaceOff().FramesToKillMe() < Target()->FramesToBeKilledByVChasers() + 2*(int)Target()->VChasers().size() - Agent()->AvgCoolDown())
			if (Agent()->Life() <= 2*m_FaceOff.HisAttack() + 1)
			{
			///	{bw << "hey <loosing> !  " << Agent()->NameWithId() << "  FramesToKillMe = " << GetFaceOff().FramesToKillMe() << "  FramesToBeKilledByVChasers = " << Target()->FramesToBeKilledByVChasers() << endl; /*ai()->SetDelay(5000);*/}

				return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
			}

		if (Target()->Life() > Target()->PrevLife(20))
		{
		///	{bw << "hey <target healed> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(5000);}

			return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
		}


		{
			if (Agent()->Life() < Agent()->PrevLife(20))
			{
				auto Threats1 = findThreats(Agent()->IsMyUnit(), 1*32);
				int countThreats1 = Threats1.size();
				if (countThreats1 >= 2)
				{
				//	bw << "hey <surrounded> !  " << Agent()->NameWithId() << " " << Agent()->Life() << " " << countThreats1 << " " << GetFaceOff().FramesToKillMe() / countThreats1 << " " << GetFaceOff().FramesToKillMe() / countThreats1 - 10 << " compared to " << Target()->FramesToBeKilledByVChasers() << endl; //ai()->SetDelay(2000);
					//if (Target()->FramesToBeKilledByVChasers() > GetFaceOff().FramesToKillMe() / countThreats1 - 10)	// flee only if Target is far from being killed
					{
					///	bw << "--> hey <surrounded> !  " << Agent()->NameWithId() << endl; //ai()->SetDelay(2000);
						return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
					}
				}
			}
		}

		{
			auto Threats3 = findThreats(Agent()->IsMyUnit(), 3*32);
			for (auto * faceOff : Threats3)
				if (faceOff->His()->IsHisBuilding() || faceOff->His()->Is(Terran_Siege_Tank_Siege_Mode) || faceOff->His()->Is(Terran_Vulture_Spider_Mine))
				{
				///	if (faceOff->His()->IsHisBuilding()) { bw << "--> stop chasing <dangerous building> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000); }
				///	if (faceOff->His()->Is(Terran_Vulture_Spider_Mine)) { bw << "--> stop chasing <mine> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000); }
				///	if (faceOff->His()->Is(Terran_Siege_Tank_Siege_Mode)) { bw << "--> stop chasing <tank> !  " << Agent()->NameWithId() << endl; ai()->SetDelay(2000); }
					return Agent()->ChangeBehavior<Kiting>(Agent()->IsMyUnit());
				}
		}
	}

	switch (State())
	{
	case chasing:		OnFrame_chasing(); break;
	default: assert_throw(false);
	}
}


void VChasing::OnFrame_chasing()
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
	if (ai()->Frame() - m_lastFrameTouchedHim > Agent()->AvgCoolDown() + 10)
	{
		if (ai()->Frame() - m_lastFrameMovedToRandom > 10)
		{
		///	bw << Agent()->NameWithId() << " blocked: move to random position" << endl; ai()->SetDelay(500);
			m_lastFrameMovedToRandom = ai()->Frame();
			return Agent()->Move(ai()->GetVMap().RandomPosition(Agent()->Pos(), 5*32));
		}
		else return;
	}

	if (Target()->VChasers().size() >= 2)
	{
		if (m_touchedHim && (ai()->Frame() - m_chasingSince > 20))
			for (VChasing * pVChaser : Target()->VChasers())
				pVChaser->m_noMoreVChasers = true;
	}

	Vect Aacc(Agent()->Acceleration());
	Vect Tacc(Target()->Acceleration());
	Vect AT = toVect(Target()->Pos() - Agent()->Pos());

//	double normAacc = Aacc.Norm();
//	double normTacc = Tacc.Norm();
//	double normAT = AT.Norm();

	if (!Agent()->CoolDown())
	{
#if DEV
		bw->drawCircleMap(Target()->Pos(), 15, Colors::Yellow);//1
#endif
		if (ai()->Frame() - m_lastFrameAttack > 5)
		{
			m_lastFrameAttack = ai()->Frame();
///			bw << ai()->Frame() << " : " << Agent()->NameWithId() << " attack!" << endl; //ai()->SetDelay(500);
			Agent()->Attack(Target());
		}
		return;
	}
	else
	{
		m_touchedHim = true;
		m_lastFrameTouchedHim = ai()->Frame();
		if (!m_firstFrameTouchedHim) m_firstFrameTouchedHim = ai()->Frame();
//		if (normAT < 45) return;

	///	bw << ai()->Frame() << " : " << Agent()->NameWithId() << " !!!" << endl; ai()->SetDelay(5000);
	///	bw << normAT << "!!!" << endl;
	}
/*
	if (m_touchedHim)
		if (Agent()->Life() < Agent()->PrevLife(25)) return;		// keep attacking (not moving) if enemy is kiting.

	Vect uAT = AT; uAT.Normalize();
	Vect uDir = uAT;
	if (normTacc > 0.1)
	{
		Vect uTacc = Tacc; uTacc.Normalize();

#if DEV
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

#if DEV
	drawLineMap(Agent()->Pos(), dest, Colors::Grey, crop);//1
#endif

	if (normAT > 3*32)
		for (int i = 0 ; (i < 3) && (dist(Agent()->Pos(), dest) > 64) ; ++i)
			adjustDestWithAltitude(dest);

	CHECK_POS(TilePosition(dest));
	adjustDestWithBlockedTiles(Agent()->Pos(), dest);
	CHECK_POS(TilePosition(dest));

	if (queenWiseDist(Agent()->Pos(), dest) < 32)
	{
	//	{bw << "no bypassing !  " << Agent()->NameWithId() << endl; if (!ai()->Delay()) ai()->SetDelay(50);}
	//	return ChangeState(bypassing);
		return Agent()->Attack(Target());
	}

#if DEV
	drawLineMap(Agent()->Pos(), dest, Colors::Green);//1
#endif

	if (ai()->GetMap().GetArea(WalkPosition(dest)) != Target()->GetArea())
		dest = Target()->Pos();

	Agent()->Move(dest);
///	bw << Agent()->NameWithId() << "move " << dest << " !!!!!!!!!!!!!" << endl;
*/
}


} // namespace iron



