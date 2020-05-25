//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DefaultBehavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


DefaultBehavior::DefaultBehavior(MyBWAPIUnit * pAgent)
	: Behavior(pAgent, behavior_t::DefaultBehavior)
{

}


DefaultBehavior::~DefaultBehavior()
{

}


void DefaultBehavior::OnFrame_v()
{CI(this);
	if (!Agent()->Is(Terran_Vulture_Spider_Mine))
	{
		if (!Agent()->CanAcceptCommand()) return;

		if (!Agent()->Completed())
		{
			if (MyBuilding * b = Agent()->IsMyBuilding())
			{
				int deltaLife = b->Life() - b->PrevLife(10);
				if (deltaLife < -2)
					if (b->Life() < 130)
						if (b->Life() < 2*(-deltaLife))
							if (b->CanAcceptCommand())
								b->CancelConstruction();
			}

			if (!Agent()->IsMyBuilding())
				return;
		}

		if (Agent()->IsMyBuilding())
		{
			//if (Agent()->Unit()->isTraining()) return;
		}
		else
		{
			if (!Agent()->Unit()->isIdle() && Agent()->Unit()->canStop())
			{
				Agent()->Stop();
				assert_throw(Agent()->Unit()->isIdle());
			}
		}
	}

	Agent()->DefaultBehaviorOnFrame();
}



} // namespace iron



