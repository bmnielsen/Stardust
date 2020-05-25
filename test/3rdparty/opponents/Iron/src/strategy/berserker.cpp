//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "berserker.h"
#include "../behavior/defaultBehavior.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Berserker
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Berserker::Berserker()
{
}



string Berserker::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


bool Berserker::Active() const
{
	return m_active;
}


int Berserker::MineralsThreshold() const
{
	int threshold = 3000 + 2000*m_count;

	int baseLead = (int)me().Bases().size() - ai()->GetMap().BaseCount()/2;
	if (baseLead <= 0)
		threshold += 1000*(1 - baseLead);

	return threshold;
}


int Berserker::GasThreshold() const
{
	int threshold = 1000 + 500*m_count;

	int baseLead = (int)me().Bases().size() - ai()->GetMap().BaseCount()/2;
	if (baseLead <= 0)
		threshold += 300*(1 - baseLead);

	return threshold;
}


void Berserker::OnFrame_v()
{
	if (m_active)
	{
		if (!Interactive::berserker)
			if ((ai()->Frame() - m_activeSince > 500 + 100*m_count) ||
				(me().SupplyUsed() < 175) ||
				(me().SupplyUsed() < 150) && (me().MineralsAvailable() > 10000) && (me().GasAvailable() > 3000) ||
				(me().MineralsAvailable() < MineralsThreshold() - 2000) ||
				(me().GasAvailable() < GasThreshold() - 500))
			{
				m_active = false;
				m_inactiveSince = ai()->Frame();
				++m_count;
				return;
			}

#if DEV || DISPLAY_BERSERKER_TIME
	for (int i = 0 ; i < 3 ; ++i)
		bw->drawBoxScreen(Position(i, i), Position(640-1-i, 374-1-i), Colors::Red);
#endif
	}
	else
	{
		if (ai()->Frame() - m_inactiveSince > 1000 + 500*m_count)
			if (me().SupplyUsed() >= 199)
			if (me().MineralsAvailable() >= MineralsThreshold())
			if (me().GasAvailable() >= GasThreshold())
			{
				m_active = true;
				m_activeSince = ai()->Frame();
			}

		if (Interactive::berserker)
		{
			m_active = true;
			m_activeSince = ai()->Frame();
		}
	}
}


} // namespace iron



