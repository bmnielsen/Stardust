#include "Map.h"

#include <fstream>
#include <memory>
#include <Util/Sha1.h>

#include "../Config.h"

#include <BW/BWData.h>
#include "GameImpl.h"
#include "PlayerImpl.h"

#include "../../../Debug.h"

using namespace std;
namespace BWAPI
{
  Map::Map(BW::Game bwgame) : bwgame(bwgame)
  {
  }

  //----------------------------------------------- GET WIDTH ------------------------------------------------
  u16 Map::getWidth() const
  {
    return bwgame.mapTileSize_x();
  }
  //----------------------------------------------- GET HEIGHT -----------------------------------------------
  u16 Map::getHeight() const
  {
    return bwgame.mapTileSize_y();
  }
  //---------------------------------------------- GET PATH NAME ---------------------------------------------
  std::string Map::getPathName() const
  {
    std::string mapPath( bwgame.mapFileName() );

    // If the install path is included in the map path, remove it, creating a relative path
    auto path = installPath();
    if (!path.empty() && mapPath.compare(0, path.length(), path) == 0)
      mapPath.erase(0, installPath().length());

    return mapPath;
  }
  //---------------------------------------------- GET FILE NAME ---------------------------------------------
  std::string Map::getFileName() const
  {
    const char* c = bwgame.mapFileName();
    const char* r = c;
    while (*c) {
      if (*c == '/' || *c == '\\') r = c + 1;
      ++c;
    }
    return r;
  }
  //------------------------------------------------ GET NAME ------------------------------------------------
  std::string Map::getName() const
  {
    return bwgame.mapTitle();
  }
  void Map::copyToSharedMemory() const
  {
    const int width = getWidth();
    const int height = getHeight();

    GameData* const data = BroodwarImpl.server.data;
    if ( BroodwarImpl.isReplay() )
    {
      for(int x = 0; x < width; ++x)
      {
        for(int y = 0; y < height; ++y)
        {
          auto tileData = bwgame.getActiveTile(x, y);
          data->isVisible[x][y]  = tileData.bVisibilityFlags   != 255;
          data->isExplored[x][y] = tileData.bExploredFlags     != 255;
          data->hasCreep[x][y]   = tileData.bTemporaryCreep    != 0;
          data->isOccupied[x][y] = tileData.bCurrentlyOccupied != 0;
        }
      }
    }
    else
    {
      const bool completeMapInfo = Broodwar->isFlagEnabled(Flag::CompleteMapInformation);
      const u32 playerFlag = 1 << BroodwarImpl.BWAPIPlayer->getIndex();
      for(int x = 0; x < width; ++x)
      {
        for(int y = 0; y < height; ++y)
        {
          auto tileData = bwgame.getActiveTile(x, y);
          data->isVisible[x][y]   = !(tileData.bVisibilityFlags & playerFlag);
          data->isExplored[x][y]  = !(tileData.bExploredFlags & playerFlag);
          data->hasCreep[x][y]    = (data->isVisible[x][y] || completeMapInfo) && (tileData.bTemporaryCreep != 0 || tileData.bHasCreep != 0);
          data->isOccupied[x][y]  = (data->isVisible[x][y] || completeMapInfo) && tileData.bCurrentlyOccupied != 0;
        }
      }
    }
  }
  //------------------------------------------------ BUILDABLE -----------------------------------------------
  bool Map::buildable(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return false;
    return bwgame.buildable(x, y);
  }
  //------------------------------------------------ WALKABLE ------------------------------------------------
  bool Map::walkable(int x, int y) const
  {
    if ((unsigned)y >= getHeight()*4u - 4u || (unsigned)x / 4 >= getWidth())
      return false;
    if (y >= getHeight() * 4 - 8 && (x < 20 || x >= getWidth() * 4 - 20))
      return false;
    return bwgame.walkable(x, y);
  }
  //------------------------------------------------ VISIBLE -------------------------------------------------
  bool Map::visible(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return false;
    if ( BroodwarImpl.isReplay() )
      return bwgame.bVisibilityFlags(x, y) != 255;
    return !(bwgame.bVisibilityFlags(x, y) & (1 << BroodwarImpl.BWAPIPlayer->getIndex()));
  }
  //--------------------------------------------- HAS EXPLORED -----------------------------------------------
  bool Map::isExplored(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return false;
    if ( BroodwarImpl.isReplay() )
      return bwgame.bExploredFlags(x, y) != 255;
    return !(bwgame.bExploredFlags(x, y) & (1 << BroodwarImpl.BWAPIPlayer->getIndex()));
  }
  //----------------------------------------------- HAS CREEP ------------------------------------------------
  bool Map::hasCreep(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return false;
    return bwgame.hasCreep(x, y);
  }
  //---------------------------------------------- IS OCCUPIED -----------------------------------------------
  bool Map::isOccupied(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return false;
    return bwgame.isOccupied(x, y);
  }
  //--------------------------------------------- GROUND HEIGHT ----------------------------------------------
  int Map::groundHeight(int x, int y) const
  {
    if ((unsigned)x >= getWidth() || (unsigned)y >= getHeight())
      return 0;
    return bwgame.groundHeight(x, y);
  }
  //------------------------------------------ GET MAP HASH --------------------------------------------------
  std::string Map::calculateMapHash()
  {
    unsigned char hash[20];
    char hexstring[sizeof(hash)*2 + 1];
    std::vector<char> data;

    // Read file
    if ( !bwgame.getScenarioChk(data) || data.empty())
      return Map::savedMapHash = "Error_map_cannot_be_opened";

    // Calculate hash
    sha1::calc(data.data(), data.size(), hash);
    sha1::toHexString(hash, hexstring);

    return savedMapHash = hexstring;
  }
  std::string Map::getMapHash() const
  {
    return savedMapHash;
  }
  //----------------------------------------------------------------------------------------------------------
};
