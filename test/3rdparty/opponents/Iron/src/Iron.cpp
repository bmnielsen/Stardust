//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "Iron.h"
#include "territory/vgridMap.h"
#include "strategy/strategy.h"
#include "strategy/strat.h"
#include "behavior/harassing.h"
#include "behavior/fleeing.h"
#include "behavior/mining.h"
#include "units/production.h"
#include "infoDrawer.h"
#include "interactive.h"
#include "debug.h"
#include <iostream>

using namespace Filter;
namespace { auto & bw = Broodwar; }


namespace iron
{


Iron * Iron::m_pInstance = nullptr;


Iron::Iron()
  :m_Map(BWEM::Map::Instance()), m_Me('-'), m_Him('-')
{
	m_pInstance = this;
	m_delay = 0;
	bw->setLocalSpeed(0);
}

Iron::~Iron()
{

}


void Iron::SetDelay(delay_t delay)
{::unused(delay);
#if DEV || INTERACTIVE
	m_delay = delay;
	bw->setLocalSpeed(delay);
#endif
}


void Iron::Update()
{
	m_frame = bw->getFrameCount();
	m_time = bw->elapsedTime();

	exceptionHandler("Critters().Update()", 2000, [this]()
	{
		Critters().Update();
	});

	GetVMap().Update();
	Him().Update();
	Me().Update();	// keep this call after Him().Update(), as Me().Update() depends on Him().Update()

	ai()->GetGridMap().UpdateStats();
}


void Iron::RunExperts()
{
	Me().RunExperts();
}


void Iron::RunBehaviors()
{
	Me().RunBehaviors();
}


void Iron::RunProduction()
{
	Me().RunProduction();
}


void Iron::onStart()
{
	exceptionHandler("onStart", 2000, [this]()
	{
	///	Log << Frame() << " onStart : " << endl;

	//	bw << bw->getRevision() << endl;
#if DEV
		Log << "The map is " << bw->mapHash() << endl;
#endif

		bw->enableFlag(Flag::UserInput);
	//	bw->enableFlag(Flag::CompleteMapInformation);

		bw->setCommandOptimizationLevel(1);

		if (bw->isReplay())
		{
			bw << "The following players are in this replay:" << endl;

			for (auto p : bw->getPlayers())
				if (!p->isObserver())
					bw << p->getName() << ", playing as " << p->getRace() << endl;
		}
		else
		{
		//	if (bw->enemy()) bw << "The matchup is " << bw->self()->getRace() << " vs " << bw->enemy()->getRace() << endl;

			m_latency = bw->getLatencyFrames();

			GetMap().Initialize();
			GetMap().FindBasesForStartingLocations();
#if BWEM_USE_MAP_PRINTER
			MapPrinter::Initialize(&GetMap());
			printMap(GetMap());
#endif

			m_pVMap = make_unique<VMap>(&GetMap());
			m_pGridMap = make_unique<VGridMap>(&GetMap());

#if BWEM_USE_MAP_PRINTER && !INTERACTIVE
			GetVMap().PrintAreaChains();
			GetVMap().PrintEnlargedAreas();
//			GetVMap().PrintImpasses();
#endif

			Me().Initialize();

			m_pStrategy = make_unique<Strategy>();

			MapDrawer::ProcessCommand("hide all");

			bw->setLocalSpeed(Delay());
		//	bw->sendText("be ready!");
		}
	});
}


void Iron::onEnd(bool isWinner)
{
	exceptionHandler("onEnd", 5000, [this, isWinner]()
	{
	///	Log << Frame() << " onEnd : " << endl;

		if (bw->isReplay())
		{
		}
		else
		{
			if (isWinner)
			{
				//bw->sendText("that was interesting!");
			}
			else
			{
				//bw->sendText("you surprised me!");
			}

			while (!Me().Units().empty())
				Me().RemoveUnit(Me().Units().back()->Unit());

			while (!Me().Buildings().empty())
				Me().RemoveBuilding(Me().Buildings().back()->Unit());
		}
	});
}


void Iron::onFrame()
{
///	if (bw->getFrameCount() < 4000) return;

	if (bw->isReplay())
	{
		bw->drawTextScreen(0, 3, "%cF = %d", InfoDrawer::Color::timeText, bw->getFrameCount());
	}
	else
	{
		if (bw->isReplay() || bw->isPaused() || !bw->self() || HeHasLeft())
			return;

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["Update"].Starting();
#endif
		exceptionHandler("Iron::Update", 2000, [this]()
		{
			Update();
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["Update"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunProduction"].Starting();
#endif
		exceptionHandler("Iron::RunProduction", 2000, [this]()
		{
			RunProduction();
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunProduction"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunExperts"].Starting();
#endif
		exceptionHandler("Iron::RunExperts", 2000, [this]()
		{
			RunExperts();
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunExperts"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["strategy 1"].Starting();
#endif
		exceptionHandler("Strategy::OnFrame(true)", 2000, [this]()
		{
			GetStrategy()->OnFrame(bool("preBehavior"));
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["strategy 1"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunBehaviors"].Starting();
#endif
		exceptionHandler("Iron::RunBehaviors", 2000, [this]()
		{
			RunBehaviors();
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["RunBehaviors"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["drawMap"].Starting();
#endif
#if DEV || INTERACTIVE
		exceptionHandler("drawMap", 2000, [this]()
		{
			drawMap(GetMap());//1
		});
#endif
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["drawMap"].Finishing();
#endif

#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["strategy 2"].Starting();
#endif
		exceptionHandler("Strategy::OnFrame(false)", 2000, [this]()
		{
			GetStrategy()->OnFrame(!bool("preBehavior"));
		});
#if DEV && BWEM_USE_WINUTILS
		m_OnFrameTimes["strategy 2"].Finishing();
#endif


#if DEV || DISPLAY_SCV_FLEEING || DISPLAY_PRODUCTION || DISPLAY_WALLS || INTERACTIVE
		exceptionHandler("drawInfos", 2000, [this]()
		{
			drawInfos();//1
		});
#endif

#if DEV || INTERACTIVE
		exceptionHandler("drawInteractive", 2000, [this]()
		{
			drawInteractive();
		});
#endif


		exceptionHandler("CheckLeaveGame", 2000, [this]()
		{
			CheckLeaveGame();
		});


	///	gridMapExample(GetMap());
	}
}


void Iron::CheckLeaveGame() const
{
	if (me().Units().empty() && (me().MineralsAvailable() < 50))
		bw->leaveGame();

	for (const auto & b : Me().Buildings(Terran_Command_Center))
		if (b->Flying())
		{
			if (Me().Units().empty())
			{
				bw->leaveGame();
				break;
			}
		}

}


void Iron::onSendText(string text)
{
	exceptionHandler("onSendText", 5000, [this, text]()
	{
	///	Log << Frame() << " onSendText : " << endl;

		if (!MapDrawer::ProcessCommand(text))
		if (!Interactive::ProcessCommand(text))
		if (!InfoDrawer::ProcessCommand(text))
		{
			int delay;
			if (from_string(text, delay))	SetDelay(delay);
			else							bw->sendText("%s", text.c_str());
		}
	});
}


void Iron::onReceiveText(BWAPI::Player player, string text)
{
	exceptionHandler("onReceiveText", 5000, [this, player, text]()
	{
	///	Log << Frame() << " onReceiveText : " << endl;

		// Parse the received text
	//	bw << player->getName() << " said \"" << text << "\"" << endl;
	});
}


void Iron::onPlayerLeft(BWAPI::Player player)
{
	exceptionHandler("onPlayerLeft", 5000, [this, player]()
	{
	///	Log << Frame() << " onPlayerLeft : " << endl;

		// Interact verbally with the other players in the game by
		// announcing that the other player has left.
	//	bw->sendText("Goodbye %s!", player->getName().c_str());

		m_heHasLeft = true;
	});
}


void Iron::onNukeDetect(BWAPI::Position target)
{
	exceptionHandler("onNukeDetect", 5000, [this, target]()
	{
	///	Log << Frame() << " onNukeDetect : " << endl;

	});
}


void Iron::onUnitDiscover(BWAPI::Unit unit)
{
	exceptionHandler("onUnitDiscover", 5000, [this, unit]()
	{
	///	Log << Frame() << " onUnitDiscover : " << endl;

	});
}


void Iron::onUnitEvade(BWAPI::Unit unit)
{
	exceptionHandler("onUnitEvade", 5000, [this, unit]()
	{
	///	Log << Frame() << " onUnitEvade : " << endl;

	});
}


void Iron::onUnitShow(BWAPI::Unit unit)
{
	exceptionHandler("onUnitShow", 5000, [this, unit]()
	{
		if (bw->isReplay())
		{
		}
		else if (!HeHasLeft())
		{
		//	bw << Frame() << " onUnitShow : " << unit->getType() << " #" << unit->getID() << endl;
			
			if (unit->getPlayer() != bw->self())
			{
				if (unit->getType().isNeutral())
				{
					if (unit->getType().isCritter())		Critters().AddUnit(unit);
				}
				else if (!unit->getType().isSpecialBuilding())
					if (unit->getPlayer() == bw->enemy())
					{

					//	bw << Frame() << " onUnitShow : " << unit->getType() << " #" << unit->getID() << endl;
						if (unit->getType().isBuilding())	Him().OnShowBuilding(unit);
						else								Him().OnShowUnit(unit);
					}
			}
		}
	});
}


void Iron::onUnitHide(BWAPI::Unit unit)
{
	exceptionHandler("onUnitHide", 5000, [this, unit]()
	{
		if (bw->isReplay())
		{
		}
		else if (!HeHasLeft())
		{
		///	bw << Frame() << " onUnitHide : " << unit->getType() << " #" << unit->getID() << endl;

			if (unit->getPlayer() != bw->self())
			{
				if (!unit->getInitialType().isMineralField())
				if (!unit->getType().isSpecialBuilding())
				{
				///	bw << Frame() << " onUnitHide : " << unit->getType() << " #" << unit->getID() << endl;
					if (unit->getInitialType().isCritter()) Critters().RemoveUnit(unit);
					else
					{
						Him().OnHideBuilding(unit);
						Him().OnHideUnit(unit);
					}
				}
			}
		}
	});
}


void Iron::onUnitCreate(BWAPI::Unit unit)
{
	exceptionHandler("onUnitCreate", 5000, [this, unit]()
	{
		if (bw->isReplay())
		{
			// if we are in a replay, then we will print out the build order of the structures
			if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
			{
				int seconds = Frame() / 24;
				int minutes = seconds / 60;
				seconds %= 60;
				bw->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
			}
		}
		else
		{
		///	bw << Frame() << " onUnitCreate : " << unit->getType() << " #" << unit->getID() << endl;

			if (unit->getPlayer() == bw->self())
			{
				if (unit->getType().isBuilding())	Me().AddBuilding(unit);
				else								//if (unit->getType() == Terran_Vulture_Spider_Mine) Him().OnShowUnit(unit); else
													Me().AddUnit(unit);
			}
		}
	});
}


void Iron::onUnitDestroy(BWAPI::Unit unit)
{
	exceptionHandler("onUnitDestroy", 5000, [this, unit]()
	{
		if (bw->isReplay())
		{
		}
		else
		{
			/// bw << Frame() << " onUnitDestroy " << unit->getType() << " #" << unit->getID() << " " << unit->getPlayer() << " " << unit->getPlayer()->getName() << endl;

			if (unit->getPlayer() == bw->self())
				if (unit->getType().isBuilding())	Me().RemoveBuilding(unit);
				else								//if (unit->getType() == Terran_Vulture_Spider_Mine) Him().OnHideUnit(unit); else
													Me().RemoveUnit(unit);
			else if (unit->getPlayer() == bw->enemy())
			{
				Him().OnDestroyed(unit);
			}
			else
			{
			///	bw << Frame() << " onUnitDestroy " << unit->getType() << " #" << unit->getID() << " " << unit->getTilePosition() << " " << unit->getPlayer()->getName() << endl;
				if (unit->getType().isMineralField())			OnMineralDestroyed(unit);
				else if (unit->getType().isSpecialBuilding())	OnStaticBuildingDestroyed(unit);
			}
		}
	});
}


void Iron::onUnitMorph(BWAPI::Unit unit)
{
	exceptionHandler("onUnitMorph", 5000, [this, unit]()
	{
		if (bw->isReplay())
		{
			// if we are in a replay, then we will print out the build order of the structures
			if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
			{
				int seconds = Frame() / 24;
				int minutes = seconds / 60;
				seconds %= 60;
				bw->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
			}
		}
		else
		{
			/// bw << Frame() << " onUnitMorph : " << unit->getType() << " #" << unit->getID() << endl;

			if (unit->getPlayer() == bw->self())
			{
				if (unit->getType().isRefinery()) Me().AddBuilding(unit);
				else if (unit->getType() == Terran_Siege_Tank_Tank_Mode) Me().SetTankModeType(unit);
				else if (unit->getType() == Terran_Siege_Tank_Siege_Mode) Me().SetSiegeModeType(unit);
				else
				{
					assert_throw(false);
				}
			}
			else if (!HeHasLeft())
			{
				if (unit->getType().isSpecialBuilding())
				{
					if (unit->getType() == Resource_Vespene_Geyser)
					{
						Me().RemoveBuilding(unit, check_t::no_check);
						Him().RemoveBuilding(unit, bool("informOthers"));
					}
				}
				else
				{
					Him().RemoveUnit(unit, bool("informOthers"));
					Him().RemoveBuilding(unit, bool("informOthers"));
					if (unit->getType().isBuilding())	Him().AddBuilding(unit);
					else								Him().AddUnit(unit);
				}
			}
		}
	});
}


void Iron::onUnitRenegade(BWAPI::Unit unit)
{
	exceptionHandler("onUnitRenegade", 5000, [this, unit]()
	{
	///	Log << Frame() << " onUnitRenegade : " << endl;
	 ///bw << Frame() << " onUnitRenegade : " << unit->getType() << " #" << unit->getID() << " " << unit->getPlayer() << " " << unit->getPlayer()->getName()  << " " << unit->getPlayer()->getType() << endl;
		if (bw->isReplay())
		{
		}
		else
		{
			if (unit->getType().isAddon())
			{
				if (unit->getPlayer() == bw->self())
				{
					Me().AddBuilding(unit);
				}
				else if (unit->getPlayer() == bw->enemy())
				{
					Him().AddBuilding(unit);
				}
				else
				{
					Me().RemoveBuilding(unit, check_t::no_check);
					Him().RemoveBuilding(unit, bool("informOthers"));
				}
			}
			else if (!unit->getType().isBuilding())
			{
				if (unit->getPlayer() == bw->enemy())
				{
					Me().RemoveUnit(unit);
					Him().AddUnit(unit);

				///	bw << "Mind control on " << unit->getType() << " #" << unit->getID() << endl;
				///	ai()->SetDelay(500);
				}
			}
		}
	});
}


void Iron::onSaveGame(string gameName)
{
	exceptionHandler("onSaveGame", 5000, [this, gameName]()
	{
	///	Log << Frame() << " onSaveGame : " << endl;

		bw << "The game was saved to \"" << gameName << "\"" << endl;
	});
}


void Iron::onUnitComplete(BWAPI::Unit unit)
{
	exceptionHandler("onUnitComplete", 5000, [this, unit]()
	{
	///	Log << Frame() << " onUnitComplete : " << endl;

	});
}


void Iron::OnMineralDestroyed(BWAPI::Unit u)
{
	Mineral * m = GetMap().GetMineral(u);
	assert_throw(m);

	{	// TODO: like OnOtherBWAPIUnitDestroyed

		for (auto * runaway : Fleeing::Instances())
			runaway->OnMineralDestroyed(m);

		auto Miners = Mining::Instances();	// local copy as the call to OnMineralDestroyed(m) will modify Mining::Instances()
		for (auto * miner : Miners)
			miner->OnMineralDestroyed(m);
	}

	GetMap().OnMineralDestroyed(u);
}


void Iron::OnStaticBuildingDestroyed(BWAPI::Unit u)
{
	GetMap().OnStaticBuildingDestroyed(u);
}



	
} // namespace iron


