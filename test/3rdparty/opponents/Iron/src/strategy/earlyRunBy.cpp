//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "earlyRunBy.h"
#include "../units/him.h"
#include "../behavior/kiting.h"
#include "../behavior/scouting.h"
#include "../behavior/walking.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class EarlyRunBy
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


EarlyRunBy::EarlyRunBy()
{
}


EarlyRunBy::~EarlyRunBy()
{
	for (MyUnit * u : m_Squad)
		if (u->GetBehavior()->IsWalking())
			u->ChangeBehavior<DefaultBehavior>(u);
}


string EarlyRunBy::StateDescription() const
{
	if (!m_started) return "-";
	if (m_started) return "started";

	return "-";
}


void EarlyRunBy::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	if (m_started)
		if (MyUnit * u = pBWAPIUnit->IsMyUnit())
			if (contains(m_Squad, u))
				really_remove(m_Squad, u);
}


void EarlyRunBy::OnFrame_v()
{
	const Area * pHisArea = him().GetArea();
	if (!pHisArea) return;


	vector<HisBuilding *> HisBlockingStaticDefenses;
	for (const auto & b : him().Buildings())
		if (b->GroundThreatBuilding())
			if (b->Completed())
				if (dist(b->Pos(), him().StartingBase()->Center()) < dist(b->Pos(), me().StartingBase()->Center()))
				{
					if ((b->GetArea() != pHisArea) ||
						any_of(pHisArea->ChokePoints().begin(), pHisArea->ChokePoints().end(),
								[&b](const ChokePoint * cp){ return ext(cp)->AirDistanceFrom(b->Pos()) < 8*32; }))
						HisBlockingStaticDefenses.push_back(b.get());
					else if (b->GetArea() == pHisArea)
					{
					///	bw << "discard EarlyRunBy Y" << endl;
						return Discard();
					}
				}
	if (HisBlockingStaticDefenses.empty()) return;
	
	if (m_started)
	{
		if (m_mayGoBack && (ai()->Frame() - m_startedSince > 30))
		{
			for (MyUnit * u : m_Squad)
				if (any_of(HisBlockingStaticDefenses.begin(), HisBlockingStaticDefenses.end(), [u](HisBuilding * b)
							{ return dist(b->Pos(), u->Pos()) > 2 + dist(b->Pos(), u->PrevPos(1)) &&
										dist(b->Pos(), u->Pos()) > 4 + dist(b->Pos(), u->PrevPos(2)); }))
				{
					m_mayGoBack = false;
				///	bw << "no go back! (" << u->NameWithId() << ")" << endl;
				///	bw << dist(HisBlockingStaticDefenses.front()->Pos(), u->Pos()) << "   ";
				///	bw << dist(HisBlockingStaticDefenses.front()->PrevPos(1), u->Pos()) << "   ";
				///	bw << dist(HisBlockingStaticDefenses.front()->PrevPos(2), u->Pos()) << endl;
				}
		}

		bool tooManyBlockingStaticDefenses = HisBlockingStaticDefenses.size() > m_Squad.size() - 2;

		int dangerousMarines = 0;
		int dangerousZerglings = 0;
		int dangerousVultures = 0;
		int dangerousOtherUnits = 0;
		for (MyUnit * u : m_Squad)
			for (const auto & fo : u->FaceOffs())
				if (!fo.His()->IsHisBuilding())
				if (!fo.His()->Type().isWorker())
					if (fo.HisAttack())
					if (fo.DistanceToHisRange() < 16)
					{
						if (fo.His()->Is(Terran_Marine)) ++dangerousMarines;
						else if (fo.His()->Is(Zerg_Zergling)) ++dangerousZerglings;
						else if (fo.His()->Is(Terran_Vulture)) ++dangerousVultures;
						else ++dangerousOtherUnits;
					}

		if (tooManyBlockingStaticDefenses ||
			dangerousOtherUnits ||
			(dangerousMarines >= 3) ||
			(dangerousZerglings >= 3) ||
			(dangerousVultures >= 2) ||
			(dangerousMarines >= 3) && (dangerousVultures >= 1))
		{
			if (m_mayGoBack)
			{
			///	if (!tooManyBlockingStaticDefenses) bw << "Discard cause dangerousUnits" << endl;
			///	else								bw << "Discard cause several " << HisBlockingStaticDefenses.front()->Type().getName() << endl;
				return Discard();
			}
		}

		for (MyUnit * u : m_Squad)
		{
			if (u->GetArea() == pHisArea)
			{
				if (dist(u->Pos(), him().StartingBase()->Center()) < 10*32)
				{
					u->SetStayInArea();
					u->ChangeBehavior<DefaultBehavior>(u);
				}
			}
			else if (m_mayGoBack && (ai()->Frame() - m_startedSince > 40))
			{
				int d = roundedDist(me().StartingBase()->Center(), u->Pos());
				if ((d < roundedDist(me().StartingBase()->Center(), u->PrevPos(10)) - 20) &&
					(d < roundedDist(me().StartingBase()->Center(), u->PrevPos(20)) - 40) &&
					(d < roundedDist(me().StartingBase()->Center(), u->PrevPos(30)) - 60))
				{
				///	bw << u->NameWithId() << " go back cause stuck" << endl;
				///	bw << dist(u->Pos(), me().StartingBase()->Center()) << "   ";
				///	bw << dist(u->PrevPos(1), me().StartingBase()->Center()) << "   ";
				///	bw << dist(u->PrevPos(2), me().StartingBase()->Center()) << endl;
					u->ChangeBehavior<DefaultBehavior>(u);
				}
			}
		}
		really_remove_if(m_Squad, [](MyUnit * u){ return u->GetBehavior()->IsDefaultBehavior(); });

		if (m_Squad.empty()) m_started = false;
	}
	else
	{
		if (m_tries == 1)
		{
		///	bw << "m_tries = " << m_tries << endl;
			return Discard();
		}

		if (HisBlockingStaticDefenses.size() >= 3)
		{
		///	bw << "HisBlockingStaticDefenses = " << HisBlockingStaticDefenses.size() << endl;
			return Discard();
		}

		const int minDistToBlockingStaticDefense = 20*32;
		vector<MyUnit *> Candidates;
		vector<MyUnit *> AdvancedCandidates;		// have at least one of HisBlockingStaticDefenses in FaceOffs()
		for (auto & u : me().Units(Terran_Vulture))
			if (u->Completed() && !u->Loaded())
				if (u->Life() >= (him().IsZerg() ? 41 : him().IsTerran() ? 68 : 80))
					if (u->GetArea() != pHisArea)
					{
						HisBuilding * pHisNearestBlockingStaticDefense = nullptr;
						int minDist = minDistToBlockingStaticDefense;
						for (HisBuilding * b : HisBlockingStaticDefenses)
						{
							int d = groundDist(u->Pos(), b->Pos());
							if (d < minDist)
							{
								minDist = d;
								pHisNearestBlockingStaticDefense = b;
							}
						}

						if (pHisNearestBlockingStaticDefense)
						{
							Candidates.push_back(u.get());

							if (any_of(u->FaceOffs().begin(), u->FaceOffs().end(), [pHisNearestBlockingStaticDefense](const FaceOff & fo)
								{ return fo.His()->IsHisBuilding() == pHisNearestBlockingStaticDefense; }))
								AdvancedCandidates.push_back(u.get());
						}
					}

		if (AdvancedCandidates.empty()) return;

	///	for (MyUnit * cand : Candidates) bw->drawBoxMap(cand->Pos()-30, cand->Pos()+30, Colors::Red);
		really_remove_if(Candidates, [&AdvancedCandidates](MyUnit * cand)
		{
			return any_of(AdvancedCandidates.begin(), AdvancedCandidates.end(), [cand](MyUnit * advCand)
							{ return groundDist(cand->Pos(), advCand->Pos()) > 7*32; });
		});
	///	for (MyUnit * cand : Candidates) bw->drawBoxMap(cand->Pos()-30, cand->Pos()+30, Colors::White);
	///	for (MyUnit * advCand : AdvancedCandidates) bw->drawBoxMap(advCand->Pos()-30, advCand->Pos()+30, Colors::Green);

		size_t candidatesWanted = 2 + HisBlockingStaticDefenses.size();
		if (Candidates.size() < candidatesWanted) return;


		vector<HisUnit *> HisSoldiersNearby;
		for (MyUnit * cand : Candidates)
			for (const auto & fo : cand->FaceOffs())
				if (fo.HisAttack())
					if (HisUnit * u = fo.His()->IsHisUnit())
						if (!u->Type().isWorker())
							push_back_if_not_found(HisSoldiersNearby, u);

		if (any_of(HisSoldiersNearby.begin(), HisSoldiersNearby.end(), [](HisUnit * u)
									{ return !(u->Is(Zerg_Zergling) || u->Is(Terran_Marine) || u->Is(Terran_Vulture)); }))
		{
		///	bw << "discard EarlyRunBy X" << endl;
			return Discard();
		}


		if (HisSoldiersNearby.size() >= 1)
		{
			if (m_waitingFramesIfFewSoldiers < 0 || HisSoldiersNearby.size() > 2) 
				m_waitingFramesIfFewSoldiers = 100;
		}

		if (--m_waitingFramesIfFewSoldiers > 0) return;

		sort(Candidates.begin(), Candidates.end(), [](const MyUnit * a, const MyUnit * b){ return a->Life() > b->Life(); });
		Candidates.resize(candidatesWanted);

	///	ai()->SetDelay(500);
	///	bw << "start runby !!!" << endl;
		m_startedSince = ai()->Frame();
		m_started = true;
		m_mayGoBack = true;
		++m_tries;
		m_Squad.clear();
		for (MyUnit * u : Candidates)
		{
			u->ChangeBehavior<Walking>(u, Position(pHisArea->Top()), __FILE__ + to_string(__LINE__));
			m_Squad.push_back(u);
		}
	}
}


} // namespace iron



