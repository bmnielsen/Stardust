#pragma once
#include <set>
#include <BWAPI.h>
#include <bwem.h>

namespace BWEB::Stations
{	
	class Station
	{
		const BWEM::Base * base;
		std::set<BWAPI::TilePosition> defenses;
		BWAPI::Position resourceCentroid;
		int defenseCount = 0;

	public:
		Station(BWAPI::Position, const std::set<BWAPI::TilePosition>&, const BWEM::Base*);

		// Returns the central position of the resources associated with this base including geysers
		BWAPI::Position ResourceCentroid() const { return resourceCentroid; }

		// Returns the set of defense locations associated with this base
		const std::set<BWAPI::TilePosition>& DefenseLocations() const { return defenses; }

		// Returns the BWEM base associated with this BWEB base
		const BWEM::Base * BWEMBase() const { return base; }

		// Returns the number of defenses associated with this station
		const int getDefenseCount() const { return defenseCount; }
		void setDefenseCount(int newValue) { defenseCount = newValue; }
	};

	/// <summary> Initializes the building of every BWEB::Station on the map, call it only once per game. </summary>
	void findStations();

	/// <summary> Returns a vector containing every BWEB::Station </summary>
	std::vector<Station> & getStations();

	/// <summary> Returns the closest BWEB::Station to the given TilePosition. </summary>
	const Station * getClosestStation(BWAPI::TilePosition);
}