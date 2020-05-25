//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "patrolBases.h"
#include "strategy.h"
#include "shallowTwo.h"
#include "../units/army.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/patrollingBases.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class PatrolBases
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


PatrolBases::PatrolBases()
{
}


PatrolBases::~PatrolBases()
{
}


string PatrolBases::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void PatrolBases::OnFrame_v()
{
	if (m_active)
	{
		assert_throw(PatrollingBases::Instances().size() <= 1);

		if (PatrollingBases::Instances().empty())
		{
			m_active = false;
			m_inactiveSince = ai()->Frame();
			return;
		}
	}
	else
	{
		assert_throw(PatrollingBases::Instances().empty());

		if (him().StartingBase())
			if (ai()->Frame() - m_inactiveSince > 100)
			{
				int threshold = (him().IsProtoss() ? 3 : 9) + (me().Army().GroundLead() ? 3 : 0);
				
				if (him().IsZerg())
					if (me().Army().MyGroundUnitsAhead() > me().Army().HisGroundUnitsAhead() + 3)
						threshold = 5;

				if ((int)me().Units(Terran_Vulture).size() >= threshold)
					if (PatrollingBases * pPatroller = PatrollingBases::GetPatroller())
					{
					///	ai()->SetDelay(50);
					///	bw << Name() << " started!" << endl;
						m_active = true;
					}
			}
	}
}


} // namespace iron



