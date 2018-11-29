#include "Station.h"
#include "BWEB.h"

using namespace std;
using namespace BWAPI;

namespace BWEB::Stations
{
	namespace {
		std::vector<Station> stations;

	}
	Station::Station(const Position newResourceCenter, const set<TilePosition>& newDefenses, const BWEM::Base* newBase)
	{
		resourceCentroid = newResourceCenter;
		defenses = newDefenses;
		base = newBase;
	}

	set<TilePosition> stationDefenses(BWAPI::Race race, const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		set<TilePosition> defenses;

		const auto &offset = [&](int x, int y) {
			return here + TilePosition(x, y);
		};

		const auto &topLeft = [&]() {
			defenses.insert({
				offset(-2, -2),
				offset(2, -2),
				offset(-2, 1) });
			if (here != Map::getMainTile() && here != Map::getNaturalTile())
				defenses.insert({
				offset(4, -1),
				offset(4, 1),
				offset(4, 3),
				offset(0,3),
				offset(2, 3) });
		};

		const auto &topRight = [&]() {
			if (race == Races::Terran)
				defenses.insert({
				offset(4, -2),
				offset(0, -2) });
			else {
				defenses.insert({
				offset(4, -2),
				offset(0, -2),
				offset(4, 1) });

				if (here != Map::getMainTile() && here != Map::getNaturalTile())
					defenses.insert({
					offset(-2, -1),
					offset(-2, 1),
					offset(-2, 3),
					offset(0, 3),
					offset(2, 3) });
			}
		};

		const auto &bottomLeft = [&]() {
			defenses.insert({
				offset(-2, 3),
				offset(-2, 0),
				offset(2, 3) });

			if (here != Map::getMainTile() && here != Map::getNaturalTile())
				defenses.insert({
				offset(0, -2),
				offset(2, -2),
				offset(4, -2),
				offset(4, 0),
				offset(4, 2) });
		};

		const auto &bottomRight = [&]() {
			if (race == Races::Terran)
				defenses.insert({
				offset(0, 3),
				offset(4, 3) });
			else {
				defenses.insert({
				offset(4, 0),
				offset(0, 3),
				offset(4, 3) });

				if (here != Map::getMainTile() && here != Map::getNaturalTile())
					defenses.insert({
					offset(-2, 2),
					offset(-2, 0),
					offset(-2,-2),
					offset(0, -2),
					offset(2, -2) });
			}
		};

		if (mirrorVertical) {
			if (mirrorHorizontal)
				bottomRight();
			else
				bottomLeft();
		}
		else {
			if (mirrorHorizontal)
				topRight();
			else
				topLeft();
		}

		// Check if it's buildable (some maps don't allow for these tiles, like Destination)
		set<TilePosition>::iterator itr = defenses.begin();
		while (itr != defenses.end()) {
			auto tile = *itr;
			if (!Map::isPlaceable(UnitTypes::Protoss_Photon_Cannon, tile))
				itr = defenses.erase(itr);
			else
				++itr;
		}

		// Temporary fix for CC Addons
		if (race == Races::Terran)
			defenses.insert(here + TilePosition(4, 1));

		// Add overlap
		for (auto &tile : defenses)
			Map::addOverlap(tile, 2, 2);

		return defenses;
	}
	set<TilePosition> stationDefenses(BWAPI::Player player, const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		return stationDefenses(player->getRace(), here, mirrorHorizontal, mirrorVertical);
	}
	set<TilePosition> stationDefenses(const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
	{
		return stationDefenses(Broodwar->self(), here, mirrorHorizontal, mirrorVertical);
	}

	void findStations()
	{
		const auto addResourceOverlap = [&](Position genCenter) {
			TilePosition start(genCenter);
			for (int x = start.x - 4; x < start.x + 4; x++) {
				for (int y = start.y - 4; y < start.y + 4; y++) {
					TilePosition t(x, y);
					if (!t.isValid())
						continue;
					if (t.getDistance(start) <= 4)
						Map::addOverlap(t, 1, 1);
				}
			}
		};

		for (auto &area : Map::mapBWEM.Areas()) {
			for (auto &base : area.Bases()) {
				auto h = false, v = false;
				Position genCenter, sCenter;
				auto cnt = 0;

				for (auto &mineral : base.Minerals()) {
					genCenter += mineral->Pos();
					cnt++;
				}

				if (cnt > 0)
					sCenter = genCenter / cnt;

				for (auto &gas : base.Geysers()) {
					sCenter = (sCenter + gas->Pos()) / 2;
					genCenter += gas->Pos();
					cnt++;
				}

				if (cnt > 0)
					genCenter = genCenter / cnt;

				h = base.Center().x < sCenter.x;
				v = base.Center().y < sCenter.y;

				for (auto &m : base.Minerals())
					Map::addOverlap(m->TopLeft(), 2, 1);

				for (auto &g : base.Geysers())
					Map::addOverlap(g->TopLeft(), 4, 2);

				const Station newStation(genCenter, stationDefenses(base.Location(), h, v), &base);
				stations.push_back(newStation);
				Map::addOverlap(base.Location(), 4, 3);
				addResourceOverlap(genCenter);
			}
		}
	}

	const Station * getClosestStation(TilePosition here)
	{
		auto distBest = DBL_MAX;
		const Station* bestStation = nullptr;
		for (auto &station : stations) {
			const auto dist = here.getDistance(station.BWEMBase()->Location());

			if (dist < distBest) {
				distBest = dist;
				bestStation = &station;
			}
		}
		return bestStation;
	}

	vector<Station> & getStations() {
		return stations;
	}
}