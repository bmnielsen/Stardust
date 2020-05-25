//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "marineRush.h"
#include "../units/cc.h"
#include "../behavior/mining.h"
#include "../behavior/sniping.h"
#include "../behavior/chasing.h"
#include "../territory/stronghold.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MarineRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


MarineRush::MarineRush()
{
}


string MarineRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


int MarineRush::MaxMarines() const
{
	return 4;
}


void MarineRush::OnFrame_v()
{
	if (him().IsZerg() || him().IsProtoss()) return Discard();

	if (m_detected)
	{
		if (m_snipersAvailable)
		{
			if (me().CompletedUnits(Terran_Vulture) >= 3) return Discard();
			if ((me().CompletedUnits(Terran_Vulture) >= 1) && (me().CompletedUnits(Terran_Marine) >= 3)) return Discard();
		}

		if (!Sniping::Instances().empty())
			m_snipersAvailable = true;

		DO_ONCE
			for (const auto & b : me().Buildings(Terran_Machine_Shop))
				if (!b->Completed())
					if (b->CanAcceptCommand())
						return b->CancelConstruction();

		DO_ONCE
		{
			if (me().Buildings(Terran_Factory).size() == 2)
			{
				MyBuilding * pLatestUncompletedFactory = nullptr;
				for (const auto & b : me().Buildings(Terran_Factory))
					if (!b->Completed())
						if (!pLatestUncompletedFactory || (b->RemainingBuildTime() > pLatestUncompletedFactory->RemainingBuildTime()))
							pLatestUncompletedFactory = b.get();

				if (pLatestUncompletedFactory && (pLatestUncompletedFactory->RemainingBuildTime() > 300))
					if (pLatestUncompletedFactory->CanAcceptCommand())
						return pLatestUncompletedFactory->CancelConstruction();
			}
		}


		static frame_t lastCancel = 0;
///		bw << ai()->Frame() - lastCancel << endl;
		if (ai()->Frame() - lastCancel > 3*bw->getRemainingLatencyFrames())
			for (UnitType type : {Terran_Vulture, Terran_Marine})
				for (const auto & bb : me().Buildings(type == Terran_Marine ? Terran_Barracks : Terran_Factory))
					if (bb->Completed() && bb->CanAcceptCommand() && !bb->Unit()->isTraining())
						if ((type != Terran_Marine) || ((int)me().Units(Terran_Marine).size() < MaxMarines()))
						{
							const bool needMinerals = me().MineralsAvailable() - Cost(type).Minerals() < -3*(int)Mining::Instances().size();
							const int supplyResult = me().SupplyAvailable() - Cost(type).Supply();

							if (( needMinerals && (supplyResult + 1 >= 0)) ||
								(!needMinerals && (supplyResult + 1 == 0)))
								for (const auto & b : me().Buildings(Terran_Command_Center))
									if (b->Unit()->isTraining())
										if (b->TimeToTrain() > 75)
											if (b->CanAcceptCommand())
											{
											///	bw << "CancelTrain SCV" << endl;
											///	ai()->SetDelay(5000);
												lastCancel = ai()->Frame();
												return b->CancelTrain();
											}

							if (needMinerals && (supplyResult >= 0))
								for (const auto & b : me().Buildings(Terran_Supply_Depot))
									if (!b->Completed())
										if (b->CanAcceptCommand())
										{
										///	bw << "CancelConstruction Depot" << endl;
										///	ai()->SetDelay(5000);
											lastCancel = ai()->Frame();
											return b->CancelConstruction();
										}
						}

		// Send SCVs to fend off the marines
		if (Mining::Instances().size() >= 5)
			for (const auto & u : him().Units())
				if (!u->InFog())
					if (u->Is(Terran_Marine))
						if (u->GetArea() == me().GetArea())
							if (u->Chasers().size() < 3)
							{
								multimap<int, My<Terran_SCV> *> Candidates;
								for (My<Terran_SCV> * pSCV : me().StartingVBase()->GetStronghold()->SCVs())
									if (pSCV->Completed() && !pSCV->Loaded())
										if (pSCV->Life() >= 49)
											if (!pSCV->GetBehavior()->IsConstructing())
											if (!pSCV->GetBehavior()->IsChasing())
											if (!pSCV->GetBehavior()->IsWalking())
												Candidates.emplace(squaredDist(u->Pos(), pSCV->Pos()), pSCV);

								if (!Candidates.empty())
									return Candidates.begin()->second->ChangeBehavior<Chasing>(Candidates.begin()->second, u.get(), bool("insist"));
							}
	}
	else
	{
		if (me().CompletedUnits(Terran_Vulture) >= 2) return Discard();
		
		const bool enemyMarinesNearby = any_of(him().Units().begin(), him().Units().end(), [](const unique_ptr<HisUnit> & u)
							{ return u->Is(Terran_Marine) && (groundDist(u->Pos(), me().StartingBase()->Center()) < 32*32); });
		
		const bool enemyBunkerNearby = any_of(him().Buildings().begin(), him().Buildings().end(), [](const unique_ptr<HisBuilding> & b)
							{ return b->Is(Terran_Bunker) && (groundDist(b->Pos(), me().StartingBase()->Center()) < 40*32); });
		
		const bool enemyBarrackNearby = any_of(him().Buildings().begin(), him().Buildings().end(), [](const unique_ptr<HisBuilding> & b)
							{ return b->Is(Terran_Barracks) && (groundDist(b->Pos(), me().StartingBase()->Center()) < 40*32); });
		
		const bool enemyBarrackNearCenter = any_of(him().Buildings().begin(), him().Buildings().end(), [](const unique_ptr<HisBuilding> & b)
							{ return b->Is(Terran_Barracks) && (groundDist(b->Pos(), ai()->GetMap().Center()) < 30*32); });


		if (enemyMarinesNearby || enemyBunkerNearby || enemyBarrackNearby || enemyBarrackNearCenter)
		{
		///	ai()->SetDelay(50);
			m_detected = true;
			m_detetedSince = ai()->Frame();
		}
	}





}


} // namespace iron



