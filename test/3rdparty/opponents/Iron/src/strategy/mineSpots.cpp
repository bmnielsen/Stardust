//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "mineSpots.h"
#include "../units/factory.h"
#include "../units/army.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/raiding.h"
#include "../territory/vgridMap.h"
#include "../territory/vcp.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MineSpots
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


MineSpots::MineSpots()
{
}


MineSpots::~MineSpots()
{
}


string MineSpots::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


Position MineSpots::FindSpot() const
{
	m_filledSpots = 0;

	const CPPath & MainPath = ai()->GetVMap().MainPath();
	for (int i = 0 ; i <= (int)MainPath.size() / 2 ; ++i)
	{
		const int minesPerCP = (i <= 1) ? 3 : 2;
		const ChokePoint * cp = MainPath[i];
		vector<VChokePoint *> TwinsCP = ext(cp)->Twins();
		TwinsCP.push_back(ext(cp));
		for (VChokePoint * vcp : TwinsCP)
		{
			Position p = center(vcp->BWEMPart()->Center());

			if (groundDist(p, me().StartingBase()->Center()) >
				groundDist(p, him().StartingBase()->Center()))
				break;

			int existingMines = count_if(me().Units(Terran_Vulture_Spider_Mine).begin(), me().Units(Terran_Vulture_Spider_Mine).end(),
				[p](const unique_ptr<MyUnit> & u){ return roundedDist(u->Pos(), p) < 2*32; });

			if (existingMines < minesPerCP) return p + Position(16*existingMines-16, 0);
			else ++m_filledSpots;
		}
	}

	return Positions::None;	
}


void MineSpots::OnFrame_v()
{
	if (m_active)
	{
	//	bw << ai()->Frame() - m_lastMinePlacement << endl;
		if (ai()->Frame() - m_lastMinePlacement > 20)
		{
			int minSpiderMineCount = 3;
			if (him().IsProtoss() && !me().Army().GroundLead() && (FilledSpots() < 6))
				minSpiderMineCount = 2;
			

			for (Raiding * r : Raiding::Instances())
				if (My<Terran_Vulture> * pVulture = r->Agent()->IsMy<Terran_Vulture>())
					if (pVulture->RemainingMines() >= minSpiderMineCount)
						if (pVulture->CanAcceptCommand())
						{
							Position spot = FindSpot();
							if (spot != Positions::None)
							{
								if (groundDist(pVulture->Pos(), spot) < 16*32)
								{
								//	DO_ONCE ai()->SetDelay(1000);
								//	bw->drawCircleMap(spot, 5, Colors::Green, true);
								//	drawLineMap(spot, pVulture->Pos(), Colors::Green);

									m_lastMinePlacement = ai()->Frame();
									return pVulture->PlaceMine(spot);
								}
							}
							else if (!contains(me().EnlargedAreas(), pVulture->GetArea()))
							{
								spot = pVulture->Pos();
								int minesAround = count_if(me().Units(Terran_Vulture_Spider_Mine).begin(), me().Units(Terran_Vulture_Spider_Mine).end(),
									[spot](const unique_ptr<MyUnit> & u){ return roundedDist(u->Pos(), spot) < 8*32; });
								if (minesAround <= 1)
								{
								///	DO_ONCE ai()->SetDelay(500);
									m_lastMinePlacement = ai()->Frame();
									return pVulture->PlaceMine(spot);
								}
							}
						}
		}
	}
	else
	{
		if (him().StartingBase())
			if (me().HasResearched(TechTypes::Spider_Mines))
				if (!Raiding::Instances().empty())
				{
				///	ai()->SetDelay(50);
				///	bw << Name() << " started!" << endl;
					m_active = true;
				}
	}
}


} // namespace iron



