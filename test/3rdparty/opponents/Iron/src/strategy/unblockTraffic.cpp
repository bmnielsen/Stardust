//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "unblockTraffic.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/walking.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


static bool maybeCandidate(const MyUnit * u, int maxAltitude)
{
	if (!u->Flying())
		if (!u->Is(Terran_Vulture_Spider_Mine))
			if (!u->GetBehavior()->IsMining())
			if (!u->GetBehavior()->IsRefining())
			if (!u->GetBehavior()->IsConstructing())
			if (!u->GetBehavior()->IsFighting())
//				if (!u->Is(Terran_Siege_Tank_Siege_Mode)
				{
					CHECK_POS(u->Pos());
					if (ai()->GetMap().GetMiniTile(WalkPosition(u->Pos())).Altitude() < maxAltitude)
//						if (!u->CoolDown())
//							if (u->Life() == u->PrevLife(BWAPIUnit::sizePrevLife-1))
								return true;
				}

	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class UnblockTraffic
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


UnblockTraffic::UnblockTraffic()
{
}


UnblockTraffic::~UnblockTraffic()
{
	FreeMovedUnits();
}


string UnblockTraffic::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


vector<MyUnit *> UnblockTraffic::FindCandidates(const GridMapCell & Cell, int minCount) const
{
	vector<MyUnit *> Candidates;

	if ((int)Cell.MyUnits.size() < minCount) return Candidates;

	for (MyUnit * u : Cell.MyUnits)
		if (maybeCandidate(u, 3*32))
			Candidates.push_back(u);

	if ((int)Candidates.size() < minCount) Candidates.clear();

	return Candidates;
}


bool UnblockTraffic::Update(const GridMapCell & Cell, const vector<MyUnit *> & Candidates)
{
	bool result = false;
	vector<MyUnit *> & PreviousCandidates = m_PreviousCandidates[&Cell];
	
	if (!PreviousCandidates.empty() && !Candidates.empty())
	{
		really_remove_if(PreviousCandidates, [&Candidates](const MyUnit * u){ return !contains(Candidates, u); });
		if (PreviousCandidates.size() >= 4) result = true;
	}

	PreviousCandidates = Candidates;
	return result;
}


vector<const GridMapCell *> UnblockTraffic::Update()
{
	vector<const GridMapCell *> CandidateCells;

	for (int j = 0 ; j < ai()->GetGridMap().Height() ; ++j)
	for (int i = 0 ; i < ai()->GetGridMap().Width() ; ++i)
		if (ai()->GetMap().GetArea(ai()->GetGridMap().GetCenter(i, j)) != me().GetArea())
		{
			const GridMapCell & Cell = ai()->GetGridMap().GetCell(i, j);
			vector<MyUnit *> Candidates = FindCandidates(Cell, 4);
			if (Update(Cell, Candidates)) CandidateCells.push_back(&Cell);
		}

	return CandidateCells;
}


pair<const GridMapCell *, const ChokePoint *> UnblockTraffic::SelectCellNearCP(vector<const GridMapCell *> CandidateCells) const
{
	for (auto pCell : CandidateCells)
	{
		const vector<MyUnit *> & Candidates = m_PreviousCandidates.find(pCell)->second;

		Position avgPos(0, 0);
		for (MyUnit * u : Candidates)
			avgPos += u->Pos();

		assert_throw(!Candidates.empty());
		avgPos /= Candidates.size();

		const ChokePoint * pNearestCP = nullptr;
		int minDist = 10*32;
		for (const VChokePoint & cp : ai()->GetVMap().ChokePoints())
			if (cp.MaxAltitude() < 3*32)
			{
				int d = roundedDist(center(cp.BWEMPart()->Center()), avgPos);
				if (d < minDist)
				{
					minDist = d;
					pNearestCP = cp.BWEMPart();
				}
			}

		if (pNearestCP) return make_pair(pCell, pNearestCP);
	}

	return make_pair(nullptr, nullptr);
}


void UnblockTraffic::FreeMovedUnits()
{
	for (MyUnit * u : m_MovedUnits)
	{
		if (!u->Is(Terran_Siege_Tank_Siege_Mode))
			u->ChangeBehavior<DefaultBehavior>(u);
	}

	m_MovedUnits.clear();
}


void UnblockTraffic::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	if (MyUnit * u = pBWAPIUnit->IsMyUnit())
		if (contains(m_MovedUnits, u))
			really_remove(m_MovedUnits, u);
}


void UnblockTraffic::OnFrame_v()
{
	if (ai()->GetStrategy()->Detected<ZerglingRush>()) return;

	if (m_active)
	{
		if (ai()->Frame() - m_activeSince > 10*(int)m_MovedUnits.size() - 30)
			if (all_of(m_MovedUnits.begin(), m_MovedUnits.end(), [](MyUnit * u){ return !u->Is(Terran_Siege_Tank_Siege_Mode); }))
			{
				FreeMovedUnits();
				m_active = false;
				return;
			}

#if DEV
		for (MyUnit * u : m_MovedUnits)
			for (int r = 21 ; r <= 23 ; ++r)
				bw->drawCircleMap(u->Pos(), 20, Colors::White);

		bw->drawCircleMap(center(m_pCP->Center()), 15*32, Colors::White);
#endif
	}
	else
	{
		if (rand() % 400 == 0)
		{
			vector<const GridMapCell *> CandidateCells = Update();
			const GridMapCell * pCell;
			tie(pCell, m_pCP) = SelectCellNearCP(CandidateCells);
			if (m_pCP)
			{
				const Area * area1, *area2;
				tie(area1, area2) = m_pCP->GetAreas();
				auto & Cell1 = ai()->GetGridMap().GetCell(TilePosition(area1->Top()));
				auto & Cell2 = ai()->GetGridMap().GetCell(TilePosition(area2->Top()));
				const Area * destArea = Cell1.AvgHisUnitsAndBuildingsLast256Frames() < Cell2.AvgHisUnitsAndBuildingsLast256Frames()
										? area1 : area2;
				m_target = Position(destArea->Top());

				vector<MyUnit *> Candidates;
				for (MyUnit * u : me().Units())
					if (u->Completed() && !u->Loaded())
						if (roundedDist(u->Pos(), center(m_pCP->Center())) < 15*32)
							if (maybeCandidate(u, 6*32))
								Candidates.push_back(u);

				m_MovedUnits = m_PreviousCandidates.find(pCell)->second;
				really_remove_if(Candidates, [this](MyUnit * u){ return contains(m_MovedUnits, u); });

				for (bool changed = true ; changed ; )
				{
					changed = false;
					for (MyUnit * u : Candidates)
						if (any_of(m_MovedUnits.begin(), m_MovedUnits.end(), [u](MyUnit * v){ return dist(u->Pos(), v->Pos()) < 45; }))
						{
							m_MovedUnits.push_back(u);
							really_remove(Candidates, u);
							changed = true;
							break;
						}
				}

				if ((m_MovedUnits.size() >= 10) && 
					all_of(m_MovedUnits.begin(), m_MovedUnits.end(), [](MyUnit * u)
							{ return !u->CoolDown() && (u->Life() == u->PrevLife(BWAPIUnit::sizePrevLife-1)); }))
				{
					for (MyUnit * u : m_MovedUnits)
						u->ChangeBehavior<Walking>(u, m_target, __FILE__ + to_string(__LINE__));

				///	ai()->SetDelay(50);
				///	bw << Name() << " started!" << endl;
					m_active = true;
					m_activeSince = ai()->Frame();
				}
				else
					m_MovedUnits.clear();
			}
		}
	}
}


} // namespace iron



