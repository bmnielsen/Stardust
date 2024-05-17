//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "infoDrawer.h"
#include "units/my.h"
#include "units/army.h"
#include "units/production.h"
#include "units/factory.h"
#include "units/bunker.h"
#include "behavior/mining.h"
#include "behavior/fleeing.h"
#include "behavior/chasing.h"
#include "behavior/kiting.h"
#include "strategy/strategy.h"
#include "strategy/strat.h"
#include "strategy/walling.h"
#include "strategy/expand.h"
#include "strategy/freeTurrets.h"
#include "territory/vgridMap.h"
#include "territory/stronghold.h"
#include "Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class InfoDrawer
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

bool InfoDrawer::showTime						= DEV && true;
bool InfoDrawer::showTimes						= DEV && false;
bool InfoDrawer::showProduction					= DEV && false || DISPLAY_PRODUCTION;
bool InfoDrawer::showGame						= DEV && false;
bool InfoDrawer::showStrats						= DEV && false;
bool InfoDrawer::showUnit						= DEV && true;
bool InfoDrawer::showBuilding					= DEV && true;
bool InfoDrawer::showCritters					= DEV && true;
bool InfoDrawer::showWalls						= DEV && true || DISPLAY_WALLS;
bool InfoDrawer::showHisKnownUnit				= DEV && false;
bool InfoDrawer::showMapPlus					= DEV && true;
bool InfoDrawer::showGrid						= DEV && false;
bool InfoDrawer::showImpasses					= DEV && false || DISPLAY_SCV_FLEEING;

const BWAPI::Color InfoDrawer::Color::time				= Colors::White;
const Text::Enum InfoDrawer::Color::timeText			= Text::White;

const BWAPI::Color InfoDrawer::Color::times				= Colors::White;
const Text::Enum InfoDrawer::Color::timesText			= Text::White;

const BWAPI::Color InfoDrawer::Color::production		= Colors::White;
const Text::Enum InfoDrawer::Color::productionText		= Text::White;

const BWAPI::Color InfoDrawer::Color::game				= Colors::White;
const Text::Enum InfoDrawer::Color::gameText			= Text::White;

const BWAPI::Color InfoDrawer::Color::building			= Colors::Green;
const Text::Enum InfoDrawer::Color::buildingText		= Text::Green;

const BWAPI::Color InfoDrawer::Color::unit				= Colors::Green;
const Text::Enum InfoDrawer::Color::unitText			= Text::Green;

const BWAPI::Color InfoDrawer::Color::enemy				= Colors::Red;
const Text::Enum InfoDrawer::Color::enemyText			= Text::Red;

const BWAPI::Color InfoDrawer::Color::enemyInFog		= Colors::Grey;
const Text::Enum InfoDrawer::Color::enemyInFogText		= Text::Grey;

const BWAPI::Color InfoDrawer::Color::critter			= Colors::Grey;
const Text::Enum InfoDrawer::Color::critterText			= Text::Grey;


bool InfoDrawer::ProcessCommandVariants(const string & command, const string & attributName, bool & attribut)
{
	if (command == "show " + attributName) { attribut = true; return true; }
	if (command == "hide " + attributName) { attribut = false; return true; }
	if (command == attributName)		   { attribut = !attribut; return true; }
	return false;
}


bool InfoDrawer::ProcessCommand(const string & command)
{
#if DEV
	if (ProcessCommandVariants(command, "time", showTime))			return true;
	if (ProcessCommandVariants(command, "times", showTimes))		return true;
	if (ProcessCommandVariants(command, "p", showProduction))		return true;
	if (ProcessCommandVariants(command, "g", showGame))				return true;
	if (ProcessCommandVariants(command, "s", showStrats))			return true;
	if (ProcessCommandVariants(command, "unit", showUnit))			return true;
	if (ProcessCommandVariants(command, "building", showBuilding))	return true;
	if (ProcessCommandVariants(command, "critter", showCritters))	return true;
	if (ProcessCommandVariants(command, "walls", showWalls))		return true;
	if (ProcessCommandVariants(command, "h", showHisKnownUnit))		return true;
	if (ProcessCommandVariants(command, "map plus", showMapPlus))	return true;
	if (ProcessCommandVariants(command, "grid", showGrid))			return true;
	if (ProcessCommandVariants(command, "nogo", showImpasses))		return true;
	
	if (ProcessCommandVariants(command, "all info", showTime))
	if (ProcessCommandVariants(command, "all info", showTimes))
	if (ProcessCommandVariants(command, "all info", showProduction))
	if (ProcessCommandVariants(command, "all info", showGame))
	if (ProcessCommandVariants(command, "all info", showStrats))
	if (ProcessCommandVariants(command, "all info", showUnit))
	if (ProcessCommandVariants(command, "all info", showBuilding))
	if (ProcessCommandVariants(command, "all info", showCritters))
	if (ProcessCommandVariants(command, "all info", showWalls))
	if (ProcessCommandVariants(command, "all info", showMapPlus))
	if (ProcessCommandVariants(command, "all info", showGrid))
	if (ProcessCommandVariants(command, "all info", showImpasses))
		return true;
#endif

	return false;
}



void drawTime()
{
	int x = 3;
	int y = 0;
	bw->drawTextScreen(x, y, "%cF = %d", InfoDrawer::Color::timeText, ai()->Frame()); x += 55;
	bw->drawTextScreen(x, y, "%cT = %.2d:%.2d", InfoDrawer::Color::timeText, ai()->Time()/60, ai()->Time()%60); x += 60;
	bw->drawTextScreen(x, y, "%cFPS = %d", InfoDrawer::Color::timeText, (int)(0.5 + bw->getAverageFPS())); x += 55;
	bw->drawTextScreen(x, y, "%cAPM = %d", InfoDrawer::Color::timeText, bw->getAPM()); x += 65;
	bw->drawTextScreen(x, y, "%cdelay = %d", InfoDrawer::Color::timeText, ai()->Delay()); x += 70;

	if (auto * pFreeTurrets = ai()->GetStrategy()->Active<FreeTurrets>())
		bw->drawTextScreen(x, y, "%cFreeTurrets =  %d %d", InfoDrawer::Color::timeText, (int)pFreeTurrets->NeedManyTurrets(), (int)pFreeTurrets->NeedManyManyTurrets()); x += 70;

//	bw->drawTextScreen(x, y, "%csupply = %d", InfoDrawer::Color::timeText, bw->self()->supplyUsed() / 2);

}

void drawTimes()
{
#if DEV && BWEM_USE_WINUTILS
	int x = 3;
	int y = 15;
	for (auto it : ai()->OnFrameTimes())
	{
		bw->drawTextScreen(x, y, "%c%s: avg = %.4f ; max = %.4f ; this = %.4f", InfoDrawer::Color::timesText, it.first.c_str(), it.second.Avg(), it.second.Max(), it.second.Time()); y += 10;
	}
#endif
}


void drawBuildingBar(const MyBuilding * b, int top, int height, BWAPI::Color col, int filledSegments, int totalSegments)
{
	Position TopLeftBuildingRect = b->Pos() - Position(b->Size())/2;
	Position BottomRightBuildingRect = b->Pos() + Position(b->Size())/2;
	int x = TopLeftBuildingRect.x;
	int y = TopLeftBuildingRect.y + top;

	bw->drawBoxMap(x, y, x + Position(b->Size()).x * filledSegments / totalSegments, y+height, col, true);
	drawLineMap(Position(x, y+height), ai()->GetMap().Crop(Position(BottomRightBuildingRect.x, y+height)), col);

	for (int i = 1 ; i < totalSegments ; ++i)
	{
		int ix = x + Position(b->Size()).x * i / totalSegments;
		drawLineMap(Position(ix, y), ai()->GetMap().Crop(Position(ix, y+height)), i < filledSegments ? Colors::Black : col);
	}
}

/*
{
		bw->drawBoxMap(b->Pos() - Position(b->Size())/2, b->Pos() + Position(b->Size())/2, col);
		
		if (b->Type().canProduce()) bw->drawTextMap(b->Pos() - Position(b->Size())/2 + Position(2, 0), "%c%d", colText, b->Activity());
}
*/
void drawProductionBar(int x, int y, BWAPI::Color col, int maxLength, int tickLength, int amountByTick, int ressourcesReserved, int ressourcesAvailable)
{
	
	int reservedPartLength = min(maxLength, ressourcesReserved * tickLength / amountByTick);
	int availablePartLength = min(maxLength, ressourcesAvailable * tickLength / amountByTick);
	bw->drawBoxScreen(x, y, x+reservedPartLength, y+5, col, true);
	bw->drawBoxScreen(x+reservedPartLength, y, x+reservedPartLength+availablePartLength, y+5, col, false);

	int segments = maxLength / tickLength;
	for (int i = 0 ; i <= segments ; ++i)
		bw->drawLineScreen(x + i*tickLength, y, x + i*tickLength, y+5,
			i*tickLength < reservedPartLength ? Colors::Black : i*tickLength <= reservedPartLength+availablePartLength ? col : Colors::Grey);

}


void drawProductionBars(int x, int y, int maxLength)
{
	drawProductionBar(x, y+7*0, Colors::Cyan, maxLength, 50, 50, me().Production().MineralsReserved(), me().Production().MineralsAvailable());
	drawProductionBar(x, y+7*1, Colors::Green, maxLength, 50, 50, me().Production().GasReserved(), me().Production().GasAvailable());
	drawProductionBar(x, y+7*2, Colors::Orange, maxLength, 10, 1, me().Production().SupplyReserved(), me().Production().SupplyAvailable());

}


void drawProduction()
{
	int x = 3;
	int y = 120;

	drawProductionBars(x, y-20, 400);

	for (const Expert * e : me().Production().Experts())
		if (e->Ready())
			if (e->Priority() || DEV)
			{
				char status = e->Selected() ? '+' : e->Started() ? '-' : ' ';
				bw->drawTextScreen(x, y, "%c%c %d %s", InfoDrawer::Color::productionText, status, e->Priority(), e->ToString().c_str()); y += 10;
			}
}


void drawGame()
{
	int x = 400;
	int y = 15;

	bw->drawTextScreen(x, y, "%cME:   scans: %d", InfoDrawer::Color::gameText, me().TotalAvailableScans()); y += 10;
	bw->drawTextScreen(x, y, "%cActivity: F = %d/10", InfoDrawer::Color::gameText, me().FactoryActivity()); y += 10;
	bw->drawTextScreen(x, y, "%cArmy: Value = %d  Evolution = %d", InfoDrawer::Color::gameText, me().Army().Value(), (int)me().Army().GoodValueInTime()); y += 10;
//	bw->drawTextScreen(x, y, "%cMinerals: %d    Gas: %d    Supply: %d/%d", InfoDrawer::Color::gameText, me().MineralsAvailable(), me().GasAvailable(), me().SupplyUsed(), me().SupplyMax()); y += 10;
//	bw->drawTextScreen(x, y, "%cu: %d  b: %d", InfoDrawer::Color::gameText, me().Units().size(), me().Buildings().size()); y += 10;
	bw->drawTextScreen(x, y, "%cSCVs: workers: %d  soldiers: %d (%s%d)", InfoDrawer::Color::gameText,
								me().SCVworkers(),
								me().SCVsoldiers(),
								me().ExceedingSCVsoldiers() >= 0 ? "+" : "",
								me().ExceedingSCVsoldiers()
								); y += 10;
	
	for (int i = 0 ; i < (int)me().Bases().size() ; ++i)
	{
		const VBase * base = me().Bases()[i];
		bw->drawTextScreen(x, y, "%cB%d%s) workers: %d (%d/%d m, %d/%d r, %d/%d s, %d o)", InfoDrawer::Color::gameText,
									i,
									base->Lost() ? " lost" : "",
									base->GetStronghold()->SCVs().size(),
									base->Miners().size(), base->MaxMiners(), 
									base->Refiners().size(), base->MaxRefiners(), 
									base->Supplementers().size(), base->MaxSupplementers(),
									base->OtherSCVs()
									); y += 10;
	}
	bw->drawTextScreen(x, y, "%cVultures: %d  Mines: %d", InfoDrawer::Color::gameText, me().Units(Terran_Vulture).size(), me().Units(Terran_Vulture_Spider_Mine).size()); y += 10;
	bw->drawTextScreen(x, y, "%cTanks: %d (%d/10 of VTG)", InfoDrawer::Color::gameText, me().Units(Terran_Siege_Tank_Tank_Mode).size(), me().Army().TankRatioWanted()); y += 10;
	bw->drawTextScreen(x, y, "%cGoliaths: %d (%d/10 of VTG)", InfoDrawer::Color::gameText, me().Units(Terran_Goliath).size(), me().Army().GoliathRatioWanted()); y += 10;
	bw->drawTextScreen(x, y, "%cMarines: %d  Medics: %d (%.1f)", InfoDrawer::Color::gameText, me().Units(Terran_Marine).size(), me().Units(Terran_Medic).size(), me().MedicForce()); y += 10;
	bw->drawTextScreen(x, y, "%cWraiths: %d (%d/10)", InfoDrawer::Color::gameText, me().Units(Terran_Wraith).size(), me().Army().AirAttackOpportunity()); y += 10;
	if (me().Units(Terran_Valkyrie).size() > 0) { bw->drawTextScreen(x, y, "%cValkyries: %d", InfoDrawer::Color::gameText, me().Units(Terran_Valkyrie).size()); y += 10; }
	if (me().Units(Terran_Science_Vessel).size() > 0) { bw->drawTextScreen(x, y, "%cVessels: %d", InfoDrawer::Color::gameText, me().Units(Terran_Science_Vessel).size()); y += 10; }
	if (me().Units(Terran_Dropship).size() > 0) { bw->drawTextScreen(x, y, "%cdropships: %d", InfoDrawer::Color::gameText, me().Units(Terran_Dropship).size()); y += 10; }

	y += 10;

	bw->drawTextScreen(x, y, "%cHIM:", InfoDrawer::Color::gameText); y += 10;
	bw->drawTextScreen(x, y, "%crace = %s", InfoDrawer::Color::gameText, him().Race().getName().c_str()); y += 10;
/*
	bw->drawTextScreen(x, y, "%cu = %d (%d in fog)    b = %d (%d in fog)", InfoDrawer::Color::gameText,
			him().Units().size(),     count_if(him().Units().begin(), him().Units().end(), ([](const unique_ptr<HisUnit> & up){ return up->InFog(); })),
			him().Buildings().size(), count_if(him().Buildings().begin(), him().Buildings().end(), ([](const unique_ptr<HisBuilding> & up){ return up->InFog(); }))
			); y += 10;


	if (him().StartingBase())
		{ bw->drawTextScreen(x, y, "%cstarting Base = (%d, %d)", InfoDrawer::Color::gameText, him().StartingBase()->Location().x, him().StartingBase()->Location().y); y += 10; }
	else
		{ bw->drawTextScreen(x, y, "%cstarting Base = ?", InfoDrawer::Color::gameText); y += 10; }
*/
	if (him().MayMuta()) { bw->drawTextScreen(x, y, "%cmay muta", InfoDrawer::Color::gameText); y += 10; }
	if (him().HydraPressure()) { bw->drawTextScreen(x, y, "%cHydraPressure", InfoDrawer::Color::gameText); y += 10; }
	if (me().Army().KeepGoliathsAtHome()) { bw->drawTextScreen(x, y, "%cKeepGoliathsAtHome", InfoDrawer::Color::gameText); y += 10; }
	if (me().Army().KeepTanksAtHome()) { bw->drawTextScreen(x, y, "%cKeepTanksAtHome", InfoDrawer::Color::gameText); y += 10; }

}


void drawStrats()
{
	int x = 400;
	int y = 190;

	for (const auto & strat : ai()->GetStrategy()->Strats())
	{
		bw->drawTextScreen(x, y, "%c%s : %s", InfoDrawer::Color::productionText, strat->Name().c_str(), strat->StateDescription().c_str()); y += 10;
	}
}


void drawHisKnownUnit()
{
	int x = 400;
	int y = 15;

	for (const auto & info : him().AllUnits())
	{
		bw->drawTextScreen(x, y, "%c#%d %s (%d)",
						info.second.GetHisUnit() ? Text::White : Text::Grey,
						info.first->getID(),
						info.second.Type().getName().c_str(),
						ai()->Frame() - info.second.LastTimeVisible()
			);
		
		y += 10;
	}
}


void drawShieldsAndLife(const BWAPIUnit * u, int x, int y, Text::Enum colText)
{
	const char * lifeFormat = (u->Life() < u->MaxLife()) ? "%s/%s" : "%s%s";
	const char * shieldsFormat = (u->Shields() < u->MaxShields()) ? "(%s/%s)" : "(%s%s)";
	const string format = "%c" + (u->MaxShields() ? string(shieldsFormat) + lifeFormat : string("%s%s") + lifeFormat);

	const string life = to_string(u->Life());
	const string maxLife = (u->Life() < u->MaxLife()) ? to_string(u->MaxLife()) : "";
	const string shields = u->MaxShields() ? to_string(u->Shields()) : "";
	const string maxShields = (u->Shields() < u->MaxShields()) ? to_string(u->MaxShields()) : "";
		
	bw->drawTextMap(x, y, format.c_str(), colText, shields.c_str(), maxShields.c_str(), life.c_str(), maxLife.c_str());
}


void drawEnergy(const BWAPIUnit * u, int x, int y, Text::Enum colText)
{
	if (u->Energy() != u->MaxEnergy())	bw->drawTextMap(x, y, "%cenergy: %d / %d", colText, u->Energy(), u->MaxEnergy());
	else								bw->drawTextMap(x, y, "%cenergy: %d", colText, u->Energy());
}


void drawUnit()
{
	for (auto * u : me().Units())
	{
//		if (u->Is(Terran_Goliath)) bw->drawCircleMap(u->Pos(), u->AirRange(), Colors::Red);
//		if (u->Is(Terran_Wraith)) bw->drawCircleMap(u->Pos(), u->Sight(), Colors::Green);

		const Color col = u->GetBehavior()->GetColor();
		const Text::Enum colText = u->GetBehavior()->GetTextColor();

	///	if (u->GetStronghold()) drawLineMap(u->Pos(), u->GetStronghold()->HasBase()->BWEMPart()->Center(), Colors::Teal);

		bw->drawEllipseMap(u->Pos(), u->Type().width(), u->Type().height(), col);
		if (u->CoolDown()) bw->drawEllipseMap(u->Pos(), u->Type().width()-2, u->Type().height()-2, col);
		if (u->Unit()->isStimmed())
		{
			bw->drawEllipseMap(u->Pos(), u->Type().width()/2, u->Type().height()/2, col, bool("isSolid"));
			bw->drawTextMap(u->Pos() - 5, "%c%d", Text::Brown, u->Unit()->getStimTimer());
		}

		if (u->Unit()->isHoldingPosition()) for (int i = 0 ; i < 2 ; ++i) bw->drawTriangleMap(u->Pos()+Position(0, -u->Type().height()-i), u->Pos()+Position(-u->Type().width()*3/4-i, u->Type().height()/2+i), u->Pos()+Position(+u->Type().width()*3/4+i, u->Type().height()/2+i), col);
		
		int x = u->Pos().x + u->Type().width() + 3;
		int y = u->Pos().y - u->Type().height();

		if (auto * pMine = u->IsMy<Terran_Vulture_Spider_Mine>())
			{ bw->drawTextMap(x, y, "%c#%d %s", colText, u->Unit()->getID(), pMine->Ready() ? "<ready>" : "<>"); y += 10; }
		else
			{ bw->drawTextMap(x, y, "%c#%d %s", colText, u->Unit()->getID(), ("<" + u->GetBehavior()->FullName() + ">" + (u->GetBehavior()->GetSubBehavior() ? " *" : "")).c_str()); y += 10; }
		drawShieldsAndLife(u, x, y, colText); y += 10;
//		if (u->Is(Terran_Marine)) {bw->drawTextMap(x, y, "%c:<%d>", colText, u->NotFiringFor()); y += 10; }
		if (u->Is(Terran_Vulture)) {bw->drawTextMap(x, y, "%c:<%d>", colText, u->IsMy<Terran_Vulture>()->RemainingMines()); y += 10; }
//		bw->drawTextMap(x, y, "%c%s", colText, u->Type().getName().c_str()); y += 10;
//		if (u->Unit()->isMoving()) { bw->drawTextMap(x, y, "%cmoving", colText); y += 10; }
//		if (u->Burrowed()) { bw->drawTextMap(x, y, "%cburrowed", colText); y += 10; }
		if (u->Unit()->isCloaked()) { bw->drawTextMap(x, y, "%ccloacked", colText); y += 10; }
//		if (u->Unit()->isStuck()) { bw->drawTextMap(x, y, "%cstuck", colText); y += 10; }

		if (u->GetBehavior()->IsKiting())
		{
			if (u->GetBehavior()->IsKiting()->VultureFeast())
			{
//				bw->drawEllipseMap(u->Pos(), u->Type().width()/2, u->Type().height()/2, Colors::Black, bool("isSolid"));
//				DO_ONCE ai()->SetDelay(500);
//				bw << "VF" << endl;
			}
		}
		if (u->GetBehavior()->IsChasing())
		{
			if (u->GetBehavior()->IsChasing()->WorkerDefense()) bw->drawEllipseMap(u->Pos(), u->Type().width()/2, u->Type().height()/2, col, bool("isSolid"));

			FaceOff faceOff(u, u->GetBehavior()->IsChasing()->Target()->IsHisBWAPIUnit());
//			bw->drawTextMap(x, y, "%cFramesToKillMe: %d", colText, faceOff.FramesToKillMe()); y += 10;
			bw->drawTextMap(x, y, "%c: %d", colText, faceOff.FramesToKillMe()); y += 10;
//			bw->drawTextMap(x, y, "%cDistanceToMyRange: %d -> %d", colText, faceOff.DistanceToMyRange(), faceOff.DistanceToMyRange()*13/64); y += 10;
//			bw->drawTextMap(x, y, "%cFramesToKillHim: %d", colText, faceOff.FramesToKillHim()); y += 10;
//			bw->drawTextMap(x, y, "%cyyy: %d", colText, ai()->Frame() - u->GetBehavior()->IsChasing()->LastFrameTouchedHim()); y += 10;
			
		}

		if (u->NoFightUntil() > ai()->Frame()) { bw->drawTextMap(x, y, "%cno fight for %d", colText, u->NoFightUntil() - ai()->Frame()); y += 10; }

		if (u->GetHealer()) bw->drawBoxMap(u->Pos()-10, u->Pos()+10, Colors::Yellow);
//		bw->drawTextMap(x, y, "%cX: %d", colText, ai()->Frame() - u->LastFrameMoved()); y += 10;

//		bw->drawTextMap(x, y, "%carmor: %d", colText, u->Armor()); y += 10;
//		bw->drawTextMap(x, y, "%cattack: %d / %d", colText, u->GroundAttack(), u->AirAttack()); y += 10;
//		bw->drawTextMap(x, y, "%crange: %d / %d", colText, u->GroundRange(), u->AirRange()); y += 10;
		if (u->Is(Terran_Dropship)) bw->drawTextMap(x, y, "%cspeed: %.3f", colText, u->Speed()); y += 10;
		if (u->Is(Terran_Dropship)) bw->drawTextMap(x, y, "%cspeed: %.3f", colText, u->Acceleration().Norm()); y += 10;
//		bw->drawTextMap(x, y, "%ccool: %d", colText, u->CoolDown()); y += 10;
//		bw->drawTextMap(x, y, "%cavg cool: %d", colText, u->AvgCoolDown()); y += 10;
//		bw->drawTextMap(x, y, "%csight: %d", colText, u->Sight()); y += 10;
//		if (u->MaxEnergy()) { drawEnergy(u, x, y, colText); y += 10; }
	//	bw->drawTextMap(x, y, "%cacc = %s -- %f, %f, %f", colText, my_to_string(u->Pos() - u->PrevPos(1)).c_str(), u->Unit()->getVelocityX(), u->Unit()->getVelocityY(), u->Unit()->getAngle()); y += 10;
	//	bw->drawTextMap(x, y, "%c%d", colText, distToRectangle(u->Pos(), u->GetBehavior()->IsMining()->GetMineral()->TopLeft(), u->GetBehavior()->IsMining()->GetMineral()->Size())); y += 10;


//		for (const auto & faceOff : u->FaceOffs())
//		if (faceOff.Mine()->Is(Terran_Siege_Tank_Tank_Mode))
//		if (faceOff.His()->Is(Zerg_Lurker))
		{
//			drawLineMap(faceOff.Mine()->Pos(), faceOff.His()->Pos(), Colors::Yellow);
//			bw->drawTextMap((faceOff.Mine()->Pos() + faceOff.His()->Pos())/2, "%c    %d / %d", Text::Enum::Yellow, faceOff.FramesToKillHim(), faceOff.FramesToKillMe());
//			bw->drawTextMap((faceOff.Mine()->Pos() + faceOff.His()->Pos())/2, "%c    %d / %d", Text::Enum::Yellow, faceOff.GroundDistanceToHitHim(), faceOff.GroundDistanceToHitMe());
//			bw->drawTextMap((faceOff.Mine()->Pos() + faceOff.His()->Pos())/2 + Position(0, 10), "%c    %d / %d", Text::Enum::Yellow, faceOff.AirDistanceToHitHim(), faceOff.AirDistanceToHitMe());
//			bw->drawTextMap((faceOff.Mine()->Pos() + faceOff.His()->Pos())/2 + Position(0, 20), "%c    %d / %d", Text::Enum::Yellow, faceOff.DistanceToMyRange(), faceOff.DistanceToHisRange());
//			bw->drawTextMap((faceOff.Mine()->Pos() + faceOff.His()->Pos())/2 + Position(0, 30), "%c    %d", Text::Enum::Yellow, faceOff.HisAttack());
		}

	}

/*
	for (auto & a : me().Units(Terran_Siege_Tank_Tank_Mode))
	for (auto & b : me().Units(Terran_Siege_Tank_Tank_Mode))
		if (a.get() != b.get())
		{
			bw->drawLineMap(a->Pos(), b->Pos(), Colors::Grey);
			int a_to_b = dynamic_cast<My<Terran_Siege_Tank_Tank_Mode> *>(a.get())->CanSiegeAttack(b.get());
			int b_to_a = dynamic_cast<My<Terran_Siege_Tank_Tank_Mode> *>(b.get())->CanSiegeAttack(a.get());
			assert_throw(a_to_b == b_to_a);
			if (a_to_b)
			{
				bw->drawLineMap(a->Pos(), b->Pos(), Colors::White);
				bw->drawTextMap((a->Pos() + b->Pos()) / 2 + Position(-20, -20), "%c%d", Text::White, a_to_b);
			}
		}

	for (auto & a : me().Units(Terran_Siege_Tank_Tank_Mode))
		for (int dy = -410 ; dy <= 410 ; ++dy) 
		for (int dx = -410 ; dx <= 410 ; ++dx) //if (abs(dy) >= 300 || abs(dx) >= 300)
		{
			Position p(a->Pos().x + dx, a->Pos().y  + dy);
			if (ai()->GetMap().Valid(p))
				if (a->GetDistanceToTarget(p, Terran_Siege_Tank_Siege_Mode))
					if (a->GetDistanceToTarget(p, Terran_Siege_Tank_Siege_Mode) > 370)
						bw->drawDotMap(p, Colors::Orange);
		}
*/



	for (auto & u : him().Units())
	{
		if (u->Is(Terran_Goliath)) bw->drawCircleMap(u->Pos(), u->AirRange(), Colors::Red);
		if (u->Is(Terran_Wraith)) bw->drawCircleMap(u->Pos(), u->Sight(), Colors::Green);

//		static bool done = false;
//		if (u->Is(Protoss_Probe))
//			if (!done) { ai()->SetDelay(500); done = true; }

		const Color col = u->InFog() ? InfoDrawer::Color::enemyInFog : InfoDrawer::Color::enemy;
		const Text::Enum colText = u->InFog() ? InfoDrawer::Color::enemyInFogText : InfoDrawer::Color::enemyText;

		if (u->Is(Terran_Siege_Tank_Tank_Mode) || u->Is(Terran_Siege_Tank_Siege_Mode))
			bw->drawCircleMap(u->Pos(), 12*32, col);

//		if (u->Is(Zerg_Lurker))
//		{
//			DO_ONCE ai()->SetDelay(500);
//			bw << "Lurker !" << endl;
//		}


		bw->drawEllipseMap(u->Pos(), u->Type().width(), u->Type().height(), col);
		if (u->CoolDown()) bw->drawEllipseMap(u->Pos(), u->Type().width()-2, u->Type().height()-2, col);
		if (!u->MinesTargetingThis().empty()) bw->drawBoxMap(u->Pos() - 23, u->Pos() + 23, Colors::Orange);

		int x = u->Pos().x + u->Type().width() + 3;
		int y = u->Pos().y - u->Type().height();

		bw->drawTextMap(x, y, "%c#%d", colText, u->Unit()->getID()); y += 10;
		drawShieldsAndLife(u.get(), x, y, colText); y += 10;
		if (!u->Chasers().empty()) { bw->drawTextMap(x, y, "%ckilled by SCVs in: %d", colText, u->FramesToBeKilledByChasers()); y += 10; }
		if (!u->VChasers().empty()) { bw->drawTextMap(x, y, "%ckilled in: %d", colText, u->FramesToBeKilledByVChasers()); y += 10; }
		bw->drawTextMap(x, y, "%c%s", colText, u->Unit()->isVisible() ? "visible" : "invisible"); y += 10;
		if (!u->Unit()->isDetected()) { bw->drawTextMap(x, y, "%cNOT DETECTED", colText); y += 10; }
		if (u->Burrowed()) { bw->drawTextMap(x, y, "%cburrowed", colText); y += 10; }
		if (!u->Unit()->isTargetable()) { bw->drawTextMap(x, y, "%cnot targetable", colText); y += 10; }
//		bw->drawTextMap(x, y, "%carmor: %d", colText, u->Armor()); y += 10;
//		bw->drawTextMap(x, y, "%cattack: %d / %d", colText, u->GroundAttack(), u->AirAttack()); y += 10;
//		bw->drawTextMap(x, y, "%crange: %d / %d", colText, u->GroundRange(), u->AirRange()); y += 10;
//		bw->drawTextMap(x, y, "%cspeed: %.3f", colText, u->Speed()); y += 10;
//		bw->drawTextMap(x, y, "%ccool: %d", colText, u->CoolDown()); y += 10;
//		bw->drawTextMap(x, y, "%cavg cool: %d", colText, u->AvgCoolDown()); y += 10;
//		bw->drawTextMap(x, y, "%csight: %d", colText, u->Sight()); y += 10;
//		if (u->MaxEnergy()) { drawEnergy(u.get(), x, y, colText); y += 10; }
	}
}


void drawBuilding()
{
	for (auto * b : me().Buildings())
	{
		const Color col = InfoDrawer::Color::building;
		const Text::Enum colText = InfoDrawer::Color::buildingText;

	//	if (b->GetStronghold()) drawLineMap(b->Pos(), b->GetStronghold()->HasBase()->BWEMPart()->Center(), Colors::Orange);

		for (int i = 0 ; i < (b->GetStronghold() ? 1 : 2) ; ++i)
			bw->drawBoxMap(b->Pos() - Position(b->Size())/2 - i, b->Pos() + Position(b->Size())/2 + i, col);

		if (b->Type().canProduce()) drawBuildingBar(b, 0, 4, col, b->Activity(), 10);
		if (b->Type().canProduce()) bw->drawTextMap(b->Pos() - Position(b->Size())/2 + Position(2, 0), "%c%d", colText, b->Activity());

		int x = (b->Pos() + Position(b->Size())/2).x + 3;
		int y = (b->Pos() - Position(b->Size())/2).y;


		if (const My<Terran_Bunker> * bunker = b->IsMy<Terran_Bunker>()) { bw->drawTextMap(x, y, "%c[%d]", colText, bunker->LoadedUnits()); y += 10; }
		bw->drawTextMap(x, y, "%c#%d %s", colText, b->Unit()->getID(), b->GetBehavior()->IsDefaultBehavior() ? "" : ("<" + b->GetBehavior()->FullName() + ">" + (b->GetBehavior()->GetSubBehavior() ? " *" : "")).c_str()); y += 10;
		drawShieldsAndLife(b, x, y, colText); y += 10;
		if (b->DeltaLifePerFrame()) { bw->drawTextMap(x, y, "%cdelta: %.1f", colText, b->DeltaLifePerFrame()); y += 10; }
		if (b->Flying()) { bw->drawTextMap(x, y, "%cfly frames: %d", colText, ai()->Frame() - b->LiftedSince()); y += 10; }
//		bw->drawTextMap(x, y, "%carmor: %d", colText, b->Armor()); y += 10;
//		bw->drawTextMap(x, y, "%cattack: %d / %d", colText, b->GroundAttack(), b->AirAttack()); y += 10;
//		bw->drawTextMap(x, y, "%crange: %d / %d", colText, b->GroundRange(), b->AirRange()); y += 10;
//		bw->drawTextMap(x, y, "%csight: %d", colText, b->Sight()); y += 10;
//		if (b->MaxEnergy()) { drawEnergy(b, x, y, colText); y += 10; }
//		bw->drawTextMap(x, y, "%cIdlePosition: %d", colText, b->IdlePosition()); y += 10;
		if (b->RemainingBuildTime()) { bw->drawTextMap(x, y, "%cbuild time: %d", colText, b->RemainingBuildTime()); y += 10; }
		if (b->TimeToTrain()) { bw->drawTextMap(x, y, "%ctraining time: %d", colText, b->TimeToTrain()); y += 10; }
		if (b->TimeToResearch()) { bw->drawTextMap(x, y, "%cresearch time: %d", colText, b->TimeToResearch()); y += 10; }
	}

	for (auto & b : him().Buildings())
	{
		const Color col = b->InFog() ? InfoDrawer::Color::enemyInFog : InfoDrawer::Color::enemy;
		const Text::Enum colText = b->InFog() ? InfoDrawer::Color::enemyInFogText : InfoDrawer::Color::enemyText;

		bw->drawBoxMap(b->Pos() - Position(b->Size())/2, b->Pos() + Position(b->Size())/2, col);

		int x = (b->Pos() + Position(b->Size())/2).x + 3;
		int y = (b->Pos() - Position(b->Size())/2).y;

		bw->drawTextMap(x, y, "%c#%d", colText, b->Unit()->getID()); y += 10;
		drawShieldsAndLife(b.get(), x, y, colText); y += 10;
		if (!b->Chasers().empty()) { bw->drawTextMap(x, y, "%ckilled in: %d", colText, b->FramesToBeKilledByChasers()); y += 10; }
		bw->drawTextMap(x, y, "%carmor: %d", colText, b->Armor()); y += 10;
		bw->drawTextMap(x, y, "%cattack: %d / %d", colText, b->GroundAttack(), b->AirAttack()); y += 10;
		bw->drawTextMap(x, y, "%crange: %d / %d", colText, b->GroundRange(), b->AirRange()); y += 10;
		bw->drawTextMap(x, y, "%csight: %d", colText, b->Sight()); y += 10;
//		if (b->Flying()) { bw->drawTextMap(x, y, "%cFLYING", colText); y += 10; }
//		if (b->MaxEnergy()) { drawEnergy(b.get(), x, y, colText); y += 10; }
		if (b->RemainingBuildTime()) { bw->drawTextMap(x, y, "%cbuild time: %d", colText, b->RemainingBuildTime()); y += 10; }
	}
}


void drawCritters()
{
	for (auto & u : ai()->Critters().Get())
	{
		const Color col = InfoDrawer::Color::critter;
	//	const Text::Enum colText = InfoDrawer::Color::critterText;

		bw->drawEllipseMap(u->Pos(), u->Type().width(), u->Type().height(), col);
	}
}


void drawWalls()
{
	if (ai()->GetStrategy()->Active<Walling>())
		for (auto & cp : ai()->GetVMap().ChokePoints())
			cp.GetWall().DrawLocations();

//	for (const ExampleWall & wall : findWalls(ai()->GetMap()))
//		wall.DrawLocations();
}


void drawMapPlus()
{
	for (auto & m : ai()->GetMap().Minerals())
	{
		bw->drawTextMap(Position(m->TopLeft()) + Position(m->Size())/2 - 5, "%c%d", Text::Enum::White, m->Data());
	}
}


void drawGrid()
{
	for (int j = 0 ; j < ai()->GetGridMap().Height() ; ++j)
	for (int i = 0 ; i < ai()->GetGridMap().Width() ; ++i)
	{
		const auto & Cell = ai()->GetGridMap().GetCell(i, j);
		TilePosition Origin(i*VGridMap::cell_width_in_tiles, j*VGridMap::cell_width_in_tiles);
		const BWAPI::Color color[4] = {Colors::Green, Colors::Blue, Colors::Yellow, Colors::Red};
		bw->drawBoxMap(Position(Origin), Position(Origin + VGridMap::cell_width_in_tiles), color[2*(j&1) + (j&1 ? i&1 : 1-(i&1))]);
		bw->drawTextMap(Position(Origin) + Position(5, 5), "%c(%d)  my: %d, %d   his: %d, %d     %d / %d", Text::Enum::Cyan,
						ai()->Frame() - Cell.lastFrameVisible,
						Cell.MyUnits.size(), Cell.MyBuildings.size(), Cell.HisUnits.size(), Cell.HisBuildings.size(),
						Cell.AvgMyFreeUnitsAndBuildingsLast256Frames(), Cell.AvgHisUnitsAndBuildingsLast256Frames());
	}
}


void drawImpasses()
{
	vector<Area::id> WantedAreas;
	for (const Fleeing * pFleeing : Fleeing::Instances())
		if (pFleeing->Agent()->Is(Terran_SCV))
			push_back_if_not_found(WantedAreas, pFleeing->Agent()->GetArea()->Id());

	for (int y = 0 ; y < ai()->GetMap().Size().y ; ++y)
	for (int x = 0 ; x < ai()->GetMap().Size().x ; ++x)
	{
		TilePosition t(x, y);
		const auto & tile = ai()->GetMap().GetTile(t);

		if (!contains(WantedAreas, tile.AreaId()) && DISPLAY_SCV_FLEEING) continue;

		const int k = 8;
		if (ai()->GetVMap().Impasse(2, tile))
			drawDiagonalCrossMap(Position(t)+k, Position(t + 1)-k, Colors::Cyan);
		if (ai()->GetVMap().Impasse(1, tile))
			drawDiagonalCrossMap(1+Position(t)+k, 1+Position(t + 1)-k, Colors::Blue);
	}
}


//VGridMap::cell_width_in_tiles
void drawInfos()
{
	if (InfoDrawer::showTime) drawTime();
	if (InfoDrawer::showTimes) drawTimes();
	if (InfoDrawer::showProduction) drawProduction();
	if (InfoDrawer::showGame) drawGame();
	if (InfoDrawer::showStrats) drawStrats();
	if (InfoDrawer::showUnit) drawUnit();
	if (InfoDrawer::showBuilding) drawBuilding();
	if (InfoDrawer::showCritters) drawCritters();
	if (InfoDrawer::showWalls) drawWalls();
	if (InfoDrawer::showHisKnownUnit) drawHisKnownUnit();
	if (InfoDrawer::showMapPlus) drawMapPlus();
	if (InfoDrawer::showGrid) drawGrid();
	if (InfoDrawer::showImpasses) drawImpasses();

#if DEV

/*
	for (int y = 0 ; y < ai()->GetMap().Size().y ; ++y)
	for (int x = 0 ; x < ai()->GetMap().Size().x ; ++x)
	{
		TilePosition t(x, y);
		if (ai()->GetMap().GetTile(t).Ptr())
			drawDiagonalCrossMap(Position(t), Position(t + 1), Colors::White);
	}
*/

/*
	{
		int x = 5;
		int y = 10;
		bw->drawTextScreen(x, y, "%cGROUND LEAD:", InfoDrawer::Color::gameText); y += 10;
		bw->drawTextScreen(x, y, "%c%s", InfoDrawer::Color::gameText, me().Army().GroundLeadStatus_marines().c_str()); y += 10;
		bw->drawTextScreen(x, y, "%c%s", InfoDrawer::Color::gameText, me().Army().GroundLeadStatus_vultures().c_str()); y += 10;
		bw->drawTextScreen(x, y, "%c%s", InfoDrawer::Color::gameText, me().Army().GroundLeadStatus_mines().c_str()); y += 10;
		bw->drawTextScreen(x, y, "%cground units ahead : %d vs %d ", InfoDrawer::Color::gameText, me().Army().MyGroundUnitsAhead(), me().Army().HisGroundUnitsAhead()); y += 10;
	}
*/

	for (int y = 0 ; y < ai()->GetMap().Size().y ; ++y)
	for (int x = 0 ; x < ai()->GetMap().Size().x ; ++x)
	{
		TilePosition t(x, y);
		const auto & tile = ai()->GetMap().GetTile(t);

	///	if (ai()->GetVMap().NearBuildingOrNeutral(tile))
	///		drawDiagonalCrossMap(Position(t)+8, Position(t + 1)-8, Colors::White);

		if (ai()->GetVMap().AddonRoom(tile))
		{
			auto col = Colors::Grey;
			bw->drawBoxMap(Position(t)+8, Position(t + 1)-8, col);
		}

/*
		if (ai()->GetVMap().OutsideWall(tile))
		{
			auto col = Colors::Red;
			bw->drawBoxMap(Position(t)+4, Position(t + 1)-4, col);
		}
		if (ai()->GetVMap().InsideWall(tile))
		{
			auto col = Colors::Green;
			bw->drawBoxMap(Position(t)+6, Position(t + 1)-6, col);
		}
*/
	}

/*
	for (Mineral * m : me().StartingBase()->Minerals())
	{
		bw->drawCircleMap(me().StartingVBase()->PosBetweenMineralAndCC(m), 5, Colors::White, true);
		bw->drawCircleMap(me().StartingVBase()->PosBetweenMineralAndCC(m), 3*32, Colors::White);
	}
*/
#endif
}


} // namespace iron



