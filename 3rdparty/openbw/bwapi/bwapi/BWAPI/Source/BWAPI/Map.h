#pragma once
#include <string>
#include <Util/Types.h>

#include "BW/BWData.h"

namespace BWAPI
{
  /**
   * Interface to acces broodwar map data. Loads buildability/walkability when
   * constructed from the current map. It means it that instance of this class
   * should exist only as long as the single map is opened.
   */
  class Map
  {
    public :
      Map(BW::Game bwgame);
      // Gets file name of the currently opened map by broodwar
      std::string getFileName() const;
      std::string getPathName() const;
      std::string getName() const;

      // Width of the current map in terms of build tiles
      u16 getWidth() const;

      // Height of the current map in terms of build tiles
      u16 getHeight() const;

      bool buildable(int x, int y) const;
      bool walkable(int x, int y) const;
      bool visible(int x, int y) const;
      bool isExplored(int x, int y) const;
      bool hasCreep(int x, int y) const;
      bool isOccupied(int x, int y) const;
      int  groundHeight(int x, int y) const;

      // Returns a value that represents the map's terrain.
      std::string calculateMapHash();
      std::string getMapHash() const;
      void copyToSharedMemory() const;

    private :
      BW::Game bwgame;

      std::string savedMapHash;
  };
}
