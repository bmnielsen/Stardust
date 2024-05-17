//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "varea.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/expand.h"
#include "../behavior/constructing.h"
#include "../Iron.h"
#include <numeric>

namespace { auto & bw = Broodwar; }

namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VArea
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


VArea * VArea::Get(const BWEM::Area * pBWEMPart)
{
	return static_cast<VArea *>(pBWEMPart->Ext());
}


VArea::VArea(const BWEM::Area * pBWEMPart)
	: m_pBWEMPart(pBWEMPart)
{
	assert_throw(!ext(m_pBWEMPart));
	m_pBWEMPart->SetExt(this);
}


VArea::~VArea()
{
	m_pBWEMPart->SetExt(nullptr);

}


void VArea::Initialize()
{
	ComputeChain();

	ComputeEnlargedArea();

	ComputeEnlargedAreaChokePoints();

	if (!BWEMPart()->Bases().empty())
	{
		ComputeHotCP();
		ComputeDefenseCP();
		ComputeSentryPosition();
//		ComputeProtossWalling();
	}
}


void VArea::ComputeChain(const VArea * pFrom)
{
	const int accessibleNeighbours = BWEMPart()->AccessibleNeighbours().size();

	if (m_pChain) return;													// already computed
	if (accessibleNeighbours == 0 || accessibleNeighbours >= 3) return;		// not in a chain

	// m_pChain is a shared pointer, so we first try to make it point to an existing adjacent AreaChain.
	for (const Area * area : BWEMPart()->AccessibleNeighbours())
		if (auto pChain = ext(area)->m_pChain)
		{
			m_pChain = pChain;
			break;
		}

	// If no existing adjacent AreaChain was found, we create it.
	if (!m_pChain) m_pChain = make_shared<AreaChain>();

	if (accessibleNeighbours == 1) m_pChain->m_leaf = true;

	// Add this to the shared AreaChain.
	if (!pFrom || (pFrom == m_pChain->m_Areas.back()))
		m_pChain->m_Areas.push_back(this);
	else
		m_pChain->m_Areas.insert(m_pChain->m_Areas.begin(), this);

	// recursive call
	for (const Area * area : BWEMPart()->AccessibleNeighbours())
		ext(area)->ComputeChain(this);
}


void VArea::ComputeEnlargedArea()
{
	if (BWEMPart()->Bases().empty()) return;

	m_EnlargedArea.push_back(BWEMPart());
	for (const auto & it : BWEMPart()->ChokePointsByArea())
	{
		const Area * pNeighbour = it.first;
		const vector<ChokePoint> & ChokePoints = *it.second;

		int maxCPlength = 0;
		for (const ChokePoint & cp : ChokePoints)
			if ((ext(&cp)->Length() > maxCPlength) && !cp.Blocked())
				maxCPlength = ext(&cp)->Length();

		if (maxCPlength >= 5*32)
			if (pNeighbour->Geysers().empty() && pNeighbour->Minerals().empty())
				if (pNeighbour->MaxAltitude()*2 < 2 * maxCPlength)
					if (pNeighbour->AccessibleNeighbours().size() <= 2)
						if (all_of(pNeighbour->ChokePointsByArea().begin(), pNeighbour->ChokePointsByArea().end(), [this, maxCPlength]
								(const pair<const Area *, const vector<ChokePoint> *> & p)
								{ return	(p.first == BWEMPart()) ||
											all_of(p.second->begin(), p.second->end(), [maxCPlength](const ChokePoint & cp)
												{ return cp.Blocked() || (ext(&cp)->Length() <= maxCPlength*3/4); })
								; }))
							m_EnlargedArea.push_back(pNeighbour);
	}
}


void VArea::ComputeEnlargedAreaChokePoints()
{
	for (const Area * area : EnlargedArea())
		for (const ChokePoint * cp : area->ChokePoints())
			if (!(contains(EnlargedArea(), cp->GetAreas().first) && contains(EnlargedArea(), cp->GetAreas().second)))
				m_EnlargedAreaChokePoints.push_back(cp);
}


void VArea::ComputeHotCP()
{
	if (none_of(BWEMPart()->Bases().begin(), BWEMPart()->Bases().end(), [](const Base & base){ return base.Starting(); }))
		return;

	map<const ChokePoint *, int> map_CP_score;
	for (TilePosition enemyMain : ai()->GetMap().StartingLocations())
		if (ai()->GetMap().GetArea(enemyMain) != BWEMPart())
		{
			CPPath path = ai()->GetMap().GetPath(center(BWEMPart()->Top()), center(enemyMain));
			for (const ChokePoint * cp : EnlargedAreaChokePoints())
				if (contains(path, cp))
					++map_CP_score[cp];
		}

	if (!map_CP_score.empty())
		m_pHotCP = max_element(map_CP_score.begin(), map_CP_score.end(), compare2nd())->first;

///	if (m_pHotCP) MapPrinter::Get().Circle(m_pHotCP->Center(), 10, MapPrinter::Color(0, 255, 255), MapPrinter::fill);

	
}


void VArea::ComputeDefenseCP()
{
	if (none_of(BWEMPart()->Bases().begin(), BWEMPart()->Bases().end(), [](const Base & base){ return base.Starting(); }))
		return;

	const ChokePoint * pCandidate = nullptr;
	for (const ChokePoint * cp : EnlargedAreaChokePoints())
		if (!cp->Blocked())
			for (TilePosition opponentMain : ai()->GetMap().StartingLocations())
				if (ai()->GetMap().GetArea(opponentMain) != BWEMPart())
				{
					if (!ai()->GetMap().GetPath(center(BWEMPart()->Top()), center(opponentMain)).empty())
						if (!pCandidate) { pCandidate = cp; break; }
						else return;
				}

	if (!pCandidate) return;

	const int maxSizeOfdefenseCP = 4*32;

	Area::UnmarkAll();
	for (const Area * a : EnlargedArea())
		a->SetMarked();

	while (ext(pCandidate)->Length() > maxSizeOfdefenseCP)
	{
		const Area * nextArea = pCandidate->GetAreas().first;
		if (nextArea->Marked()) nextArea = pCandidate->GetAreas().second;
		nextArea->SetMarked();

		const ChokePoint * nextCP = nullptr;
		for (const ChokePoint * cp : nextArea->ChokePoints())
			if (cp != pCandidate)
				if (!cp->Blocked())
					if (!nextCP) nextCP = cp;
					else return;
		
		if (!nextCP) return;
		pCandidate = nextCP;
	}

	m_pDefenseCP = pCandidate;

	for (const Area & area : ai()->GetMap().Areas())
		if (area.Marked())
			ext(&area)->m_underDefenseCP = true;

	///	MapPrinter::Get().Circle(m_pDefenseCP->Center(), 10, MapPrinter::Color(0, 255, 255));
	
}


void VArea::ComputeSentryPosition()
{
	if (none_of(BWEMPart()->Bases().begin(), BWEMPart()->Bases().end(), [](const Base & base){ return base.Starting(); }))
		return;

	vector<const CPPath *> PathsToOtherStartingBases;
	for (TilePosition opponentMain : ai()->GetMap().StartingLocations())
		if (ai()->GetMap().GetArea(opponentMain) != BWEMPart())
		{
			const CPPath & path = ai()->GetMap().GetPath(center(BWEMPart()->Top()), center(opponentMain));
			if (!path.empty())
				PathsToOtherStartingBases.push_back(&path);
		}

	if (PathsToOtherStartingBases.size() < 2) return;

	CPPath::const_iterator iMatchingEnd = PathsToOtherStartingBases.front()->end();
	for (const CPPath * path : PathsToOtherStartingBases)
	{
		auto res = mismatch(PathsToOtherStartingBases.front()->begin(), iMatchingEnd, path->begin());
		iMatchingEnd = res.first;
	}

	if (iMatchingEnd == PathsToOtherStartingBases.front()->begin()) return;
	const ChokePoint * pSentryCP = *--iMatchingEnd;

	if (ext(pSentryCP)->MaxAltitude() > UnitType(Terran_SCV).sightRange())
	{
		if (iMatchingEnd == PathsToOtherStartingBases.front()->begin()) return;
		pSentryCP = *--iMatchingEnd;
	}

	const Area * pSentryArea =
		  ai()->GetMap().GetPath(center(BWEMPart()->Top()), center(pSentryCP->GetAreas().first->Top())).size()
		> ai()->GetMap().GetPath(center(BWEMPart()->Top()), center(pSentryCP->GetAreas().second->Top())).size()
		? pSentryCP->GetAreas().first
		: pSentryCP->GetAreas().second;


	WalkPosition topLeft(pSentryArea->TopLeft());
	WalkPosition bottomRight(pSentryArea->BottomRight());

	int minScore = numeric_limits<int>::max();
	for (int y = topLeft.y ; y <= bottomRight.y ; ++y)
	for (int x = topLeft.x ; x <= bottomRight.x ; ++x)
	{
		WalkPosition w(x, y);
		const MiniTile & miniTile = ai()->GetMap().GetMiniTile(w);
		if (miniTile.AreaId() == pSentryArea->Id())
			if (miniTile.Walkable())
				if (miniTile.Altitude() <= min(UnitType(Terran_SCV).sightRange(), (int)pSentryArea->MaxAltitude()) - 16)
				{
					int d = roundedDist(w, pSentryCP->Center());
					int score = d -2*miniTile.Altitude();
					if (score < minScore)
					{
						if (all_of(pSentryArea->ChokePoints().begin(), pSentryArea->ChokePoints().end(), [pSentryCP, d, w]
								(const ChokePoint * cp)
								{
									return (cp == pSentryCP) ||
											(cp->GetAreas() == pSentryCP->GetAreas()) ||
											(cp->GetAreas() == make_pair(pSentryCP->GetAreas().second, pSentryCP->GetAreas().first)) ||
											(roundedDist(w, cp->Center()) > d + 8);
								}))
						{
							minScore = score;
							m_sentryPos = center(w);
						}
					}
				}
	}

///	MapPrinter::Get().Circle(WalkPosition(m_sentryPos), 2, MapPrinter::Color(255, 255, 255));
///	MapPrinter::Get().Circle(WalkPosition(m_sentryPos), 7*4, MapPrinter::Color(255, 255, 255));
}


bool tileInEnlargedArea(const TilePosition & t, const VArea * varea, const Area * area)
{
	CHECK_POS(t);
	const Area * a = ai()->GetMap().GetArea(t);
	return (a == area) || contains(varea->EnlargedArea(), a);
}


bool isImpasse(const Area * area)
{
	if (const AreaChain * pChain = ext(area)->IsInChain())
		if (pChain->IsLeaf())
			return true;
	return false;
}


const Area * homeAreaToHold()
{
/*
	const Area * pMainArea = me().GetArea();
	const Area * pNaturalArea = findNatural(me().StartingVBase())->BWEMPart()->GetArea();

	const int mainHeight = pMainArea->VeryHighGroundPercentage() > 50 ? 2 : pMainArea->HighGroundPercentage() > 50 ? 1 : 0;
	const int naturalHeight = pNaturalArea->VeryHighGroundPercentage() > 50 ? 2 : pNaturalArea->HighGroundPercentage() > 50 ? 1 : 0;

	if (naturalHeight > mainHeight) return pNaturalArea;

	if (const ChokePoint * pDefenseCP = ai()->Me().GetVArea()->DefenseCP())
	{
		const Area * pAreaToHold = pDefenseCP->GetAreas().first;
		if (ext(pDefenseCP->GetAreas().second)->UnderDefenseCP()) pAreaToHold = pDefenseCP->GetAreas().second;
		return pAreaToHold;
	}

	return pMainArea;
*/
	if (auto s = ai()->GetStrategy()->Active<Walling>())
		return s->GetWall()->InnerArea();

	int i = (int)me().Bases().size()-1;
	if (i > 1) i = 1;
	return me().Bases()[i]->BWEMPart()->GetArea();

}

	
} // namespace iron



