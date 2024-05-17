//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "lateCore.h"
#include "../units/him.h"
#include "../units/comsat.h"
#include "../strategy/expand.h"
#include "../behavior/kiting.h"
#include "../behavior/fighting.h"
#include "../behavior/scouting.h"
#include "../behavior/walking.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class LateCore
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


LateCore::LateCore()
{
}


LateCore::~LateCore()
{
}


string LateCore::StateDescription() const
{
	string state = "-";
	if (m_timeToReachHisBase) state = "can start";
	if (m_active) state = "active";
	return state;
}


void LateCore::OnFrame_v()
{
	if (him().IsZerg() || him().IsTerran()) return Discard();

	if (m_active)
	{
	}
	else
	{
		if (him().Units(Protoss_Dragoon).size() > 0) return Discard();

		if (me().CompletedUnits(Terran_Marine) >= 2)
			if (him().StartingBase())
				DO_ONCE
					m_timeToReachHisBase = 50 + frame_t(groundDist(center(homeAreaToHold()->Top()), him().StartingBase()->Center())
														/ UnitType(Terran_Marine).topSpeed());

		if (m_timeToReachHisBase)
		{
			const frame_t dragoonTrainingTime = 735;
			const frame_t coreBuildingTime = 965;
			const frame_t gas50time = 350;

			frame_t coreTime = 0;
			for (HisBuilding * b : him().Buildings(Protoss_Cybernetics_Core))
				coreTime = b->RemainingBuildTime();

			frame_t assimilatorTime = 0;
			for (HisBuilding * b : him().Buildings(Protoss_Assimilator))
				assimilatorTime = b->RemainingBuildTime() + gas50time;

			frame_t gatewayTime = 0;
			if (him().Buildings(Protoss_Nexus).size() == 2)
			if (him().Buildings(Protoss_Gateway).size() == 1)
			if (him().Buildings(Protoss_Cybernetics_Core).size() == 0)
			if (him().Buildings(Protoss_Robotics_Facility).size() == 0)
			if (him().Buildings(Protoss_Stargate).size() == 0)
			if (him().Buildings(Protoss_Citadel_of_Adun).size() == 0)
			if (him().Buildings(Protoss_Stargate).size() == 0)
			if (him().Buildings(Protoss_Photon_Cannon).size() == 0)
				if (him().CreationCount(Protoss_Zealot) == 0)
				if (him().CreationCount(Protoss_Shuttle) == 0)
				if (him().CreationCount(Protoss_Reaver) == 0)
					for (HisBuilding * b : him().Buildings(Protoss_Gateway))
						if (!b->Completed())
						{
							if (++m_watchLateGatewaySince > 200)
								gatewayTime = b->RemainingBuildTime() + coreBuildingTime;
						}


			frame_t dragoonTime = max(max(coreTime, assimilatorTime), gatewayTime) + dragoonTrainingTime;
//			dragoonTime = m_timeToReachHisBase + 400;

			if (dragoonTime > m_timeToReachHisBase + 800)
			{
				m_active = true;
			//	bw << "dragoonTime = " << dragoonTime << endl;
			//	bw << "m_timeToReachHisBase = " << m_timeToReachHisBase << endl;
			}
		}
	}

}


} // namespace iron



