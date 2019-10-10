#pragma once

#include <vector>
#include <unordered_map>

namespace BWAPI
{
  // Forwards
  struct GameData;
  struct GameTable;
  class Event;
  class ForceInterface;
  typedef ForceInterface* Force;
  class PlayerInterface;
  typedef PlayerInterface* Player;
  class UnitInterface;
  typedef UnitInterface* Unit;

  class Server
  {
  public:
    Server();
    ~Server();

    Server(const Server &other) = delete;
    Server(Server &&other) = delete;
    
    void      update();
    bool      isConnected() const;
    int       addEvent(const BWAPI::Event& e);
    int       addString(const char* text);
    void      clearAll();

    int       getForceID(Force force);
    Force     getForce(int id) const;
    
    int       getPlayerID(Player player);
    Player    getPlayer(int id) const;
    
    int       getUnitID(Unit unit);
    Unit      getUnit(int id) const;

    GameData  *data = nullptr;
  private:
    void onMatchStart();
    void checkForConnections();
    void initializeSharedMemory();
    void updateSharedMemory();
    void callOnFrame();
    void processCommands();
    void setWaitForResponse(bool wait);

    GameTable* gameTable = nullptr;
    int gameTableIndex = -1;
    bool connected = false;
    bool localOnly = false;

    std::vector<Force> forceVector;
    std::unordered_map<Force, int> forceLookup;

    std::vector<Player> playerVector;
    std::unordered_map<Player, int> playerLookup;

    std::vector<Unit> unitVector;
    std::unordered_map<Unit, int> unitLookup;

  };
}
