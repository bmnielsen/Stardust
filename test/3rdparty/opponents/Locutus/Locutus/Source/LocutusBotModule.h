#pragma once

#include <BWAPI.h>

namespace Locutus
{
class LocutusBotModule : public BWAPI::AIModule
{
public:

	void	onStart();
	void	onFrame();
	void	onEnd(bool isWinner);
	void	onUnitDestroy(BWAPI::Unit unit);
	void	onUnitMorph(BWAPI::Unit unit);
	void	onSendText(std::string text);
	void	onUnitCreate(BWAPI::Unit unit);
	void	onUnitDiscover(BWAPI::Unit unit);
	void	onUnitComplete(BWAPI::Unit unit);
	void	onUnitShow(BWAPI::Unit unit);
	void	onUnitHide(BWAPI::Unit unit);
	void	onUnitRenegade(BWAPI::Unit unit);

	static void setStrategy(std::string strategy);
	static void forceGasSteal();
	static std::string getStrategyName();
};

}