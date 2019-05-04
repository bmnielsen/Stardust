#pragma once

#include "BW/Position.h"
#include "Util/Types.h"

#include <cstdint>
#include <memory>
#include <functional>
#include <iterator>
#include <type_traits>
#include <vector>

namespace bwgame {
  struct unit_t;
  struct bullet_t;
}

namespace BW {


struct Player;
struct Unit;
struct Bullet;
struct Game;
struct Region;

struct openbwapi_impl;

struct GameOwner_impl;

void sacrificeThreadForUI(std::function<void()> f);

struct GameOwner {
  std::unique_ptr<GameOwner_impl> impl;
  GameOwner();
  ~GameOwner();

  Game getGame();
  void setPrintTextCallback(std::function<void(const char*)> func);
};

struct activeTile {
  u8 bVisibilityFlags = 0;
  u8 bExploredFlags = 0;
  bool bTemporaryCreep = false;
  bool bCurrentlyOccupied = false;
  bool bHasCreep = false;
};

struct UnitFinderEntry {
  void* ptr;
  int unitIndex() const;
  int searchValue() const;
};

template<typename T>
struct ValuePointer {
  T value;
  T* operator->() {
    return &value;
  }
  T operator*() {
    return value;
  }
};

struct UnitFinderIterator {
  void* ptr;

  using value_type = UnitFinderEntry;
  using pointer = ValuePointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::random_access_iterator_tag;

  using It = UnitFinderIterator;

  value_type operator*() const;
  It& operator++();
  bool operator==(It n) const;

  bool operator!=(It n)const ;
  pointer operator->() const;
  It operator++(int);

  It& operator--();
  It operator--(int);

  It& operator+=(difference_type n);
  It operator+(difference_type n) const;
  It& operator-=(difference_type n);
  It operator-(difference_type n) const;
  difference_type operator-(It n) const;
  value_type operator[](difference_type n) const;
  bool operator<(It n) const;
  bool operator>(It n) const;
  bool operator<=(It n) const;
  bool operator>=(It n) const;
};

template<size_t size, size_t alignment>
struct some_object {
  typename std::aligned_storage<size, alignment>::type obj;
  template<typename T, typename... args_T>
  void construct(args_T&&... args) {
    static_assert(sizeof(T) <= size || alignof(T) <= alignment, "some_object size or alignment too small");
    new ((T*)&obj) T(std::forward<args_T>(args)...);
  }
  template<typename T>
  void destroy() {
    as<T>().~T();
  }
  template<typename T>
  T& as() {
    static_assert(sizeof(T) <= size || alignof(T) <= alignment, "some_object size or alignment too small");
    return (T&)obj;
  }
  template<typename T>
  const T& as() const {
    static_assert(sizeof(T) <= size || alignof(T) <= alignment, "some_object size or alignment too small");
    return (const T&)obj;
  }
};

template<typename T>
struct DefaultIterator {
  some_object<sizeof(void*), alignof(void*)> obj;
  openbwapi_impl* impl = nullptr;

  using value_type = T;
  using pointer = ValuePointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::forward_iterator_tag;

  using It = DefaultIterator;

  DefaultIterator();
  ~DefaultIterator();

  value_type operator*() const;
  It& operator++();
  bool operator==(const It& n) const;

  bool operator!=(const It& n)const ;
  pointer operator->() const;
  It operator++(int);
};

using UnitIterator = DefaultIterator<Unit>;
using BulletIterator = DefaultIterator<Bullet>;

struct snapshot_impl;

struct Snapshot {
  Snapshot();
  Snapshot(Snapshot&&);
  ~Snapshot();
  Snapshot& operator=(Snapshot&&);
  std::unique_ptr<snapshot_impl> impl;
};

struct Game {
  openbwapi_impl* impl = nullptr;

  void overrideEnvVar(std::string var, std::string value);

  int g_LocalHumanID() const;

  Player getPlayer(int n) const;

  int gameType() const;
  int Latency() const;
  int ReplayHead_frameCount() const;
  int MouseX() const;
  int MouseY() const;
  int ScreenX() const;
  int ScreenY() const;
  int screenWidth() const;
  int screenHeight() const;
  void setScreenPosition(int x, int y);
  int NetMode() const;
  bool isGamePaused() const;
  bool InReplay() const;
  int getInstanceNumber() const;
  bool getScenarioChk(std::vector<char>& data) const;
  bool gameOver() const;
  void printText(const char* str) const;
  void nextFrame();
  void setGUI(bool enabled);
  void enableCheats() const;
  void saveReplay(const std::string& filename);
  std::tuple<int, int, void*> GameScreenBuffer();
  void setOnDraw(std::function<void(uint8_t*, size_t)> onDraw);
  std::tuple<int, int, uint32_t*> drawGameScreen(int x, int y, int width, int height);

  template<typename T, typename... args_T>
  void QueueCommand(args_T&&... args) {
    T buf(std::forward<args_T>(args)...);
    QueueCommand(&buf, sizeof(T));
  }
  void QueueCommand(const void* buf, size_t size);

  void leaveGame();
  bool gameClosed() const;

  u32 ReplayVision() const;
  void setReplayVision(u32);
  void setGameSpeedModifiers(int n, int value);
  void setAltSpeedModifiers(int n, int value);
  void setFrameSkip(int value);
  int getLatencyFrames() const;
  int GameSpeed() const;
  int gameSpeedModifiers(int speed) const;
  int lastTurnFrame() const;
  bool setMapFileName(const std::string& filename);
  int elapsedTime() const;
  int countdownTimer() const;
  void setReplayRevealAll(bool);
  u32 ReplayHead_gameSeed_randSeed() const;

  int mapTileSize_x() const;
  int mapTileSize_y() const;
  const char* mapFileName() const;
  const char* mapTitle() const;
  activeTile getActiveTile(int tile_x, int tile_y) const;
  bool buildable(int tile_x, int tile_y) const;
  bool walkable(int walk_x, int walk_y) const;
  bool hasCreep(int tile_x, int tile_y) const;
  bool isOccupied(int tile_x, int tile_y) const;
  int groundHeight(int tile_x, int tile_y) const;
  u8 bVisibilityFlags(int tile_x, int tile_y) const;
  u8 bExploredFlags(int tile_x, int tile_y) const;

  Unit getUnit(size_t index) const;
  Bullet getBullet(size_t index) const;

  bool triggersCanAllowGameplayForPlayer(int player) const;
  std::array<int, 12> bRaceInfo() const;
  std::array<int, 12> bOwnerInfo() const;
  const char* forceNames(size_t n) const;

  BulletIterator BulletNodeTable_begin() const;
  BulletIterator BulletNodeTable_end() const;
  UnitIterator UnitNodeList_VisibleUnit_begin() const;
  UnitIterator UnitNodeList_VisibleUnit_end() const;
  UnitIterator UnitNodeList_HiddenUnit_begin() const;
  UnitIterator UnitNodeList_HiddenUnit_end() const;
  UnitIterator UnitNodeList_ScannerSweep_begin() const;
  UnitIterator UnitNodeList_ScannerSweep_end() const;

  void setCharacterName(const std::string& name);
  void setGameTypeMelee();
  void setGameTypeUseMapSettings();
  void createSinglePlayerGame(std::function<void()> setupFunction);
  void createMultiPlayerGame(std::function<void()> setupFunction);
  void startGame();
  void switchToSlot(int n);
  int connectedPlayerCount();

  UnitFinderIterator UnitOrderingX() const;
  UnitFinderIterator UnitOrderingY() const;
  size_t UnitOrderingCount() const;

  size_t regionCount() const;
  Region getRegion(size_t index) const;
  Region getRegionAt(int x, int y) const;

  Unit createUnit(int owner, int unitType, int x, int y);
  void killUnit(Unit u);
  void removeUnit(Unit u);

  Snapshot saveSnapshot();
  void loadSnapshot(const Snapshot&);
  void setRandomSeed(uint32_t value);
  void disableTriggers();

  void sendCustomAction(const void* data, size_t size);
  void setCustomActionCallback(std::function<void(int player, const char* data, size_t size)> callback);
};

struct Player {
  int owner = -1;
  openbwapi_impl* impl = nullptr;

  int playerColorIndex() const;
  const char* szName() const;
  int nRace() const;
  int pickedRace() const;
  int nType() const;
  int nTeam() const;
  int playerAlliances(int n) const;
  Position startPosition() const;
  int PlayerVictory() const;
  int currentUpgradeLevel(int n) const;
  int maxUpgradeLevel(int n) const;
  bool techResearched(int n) const;
  bool techAvailable(int n) const;
  bool upgradeInProgress(int n) const;
  bool techResearchInProgress(int n) const;
  bool unitAvailability(int n) const;
  int minerals() const;
  int gas() const;
  int cumulativeMinerals() const;
  int cumulativeGas() const;
  int suppliesAvailable(int n) const;
  int suppliesMax(int n) const;
  int suppliesUsed(int n) const;
  int unitCountsDead(int n) const;
  int unitCountsKilled(int n) const;
  int unitCountsAll(int n) const;
  int allUnitsLost() const;
  int allBuildingsLost() const;
  int allFactoriesLost() const;
  int allUnitsKilled() const;
  int allBuildingsRazed() const;
  int allFactoriesRazed() const;
  int allUnitScore() const;
  int allKillScore() const;
  int allBuildingScore() const;
  int allRazingScore() const;
  int customScore() const;
  u32 playerVision() const;
  int downloadStatus() const;

  void setRace(int race);
  void closeSlot();
  void openSlot();

  void setUpgradeLevel(int upgrade, int level);
  void setResearched(int tech, bool researched);
  void setMinerals(int value);
  void setGas(int value);
};

struct Unit {
  bwgame::unit_t* u = nullptr;
  openbwapi_impl* impl = nullptr;

  explicit operator bool() const;

  size_t getIndex() const;
  u16 getUnitID() const;

  Position position() const;
  bool hasSprite() const;
  int visibilityFlags() const;
  int unitType() const;
  bool statusFlag(int flag) const;
  u32 visibilityStatus() const;
  bool movementFlag(int flag) const;
  int orderID() const;
  int groundWeaponCooldown() const;
  int airWeaponCooldown() const;
  int playerID() const;
  size_t buildQueueSlot() const;
  int buildQueue(size_t index) const;
  bool fighter_inHanger() const;
  Unit fighter_parent() const;
  Unit connectedUnit() const;
  int hitPoints() const;
  int resourceCount() const;
  u8 currentDirection1() const;
  int current_speed_x() const;
  int current_speed_y() const;
  Unit subUnit() const;
  int mainOrderTimer() const;
  int spellCooldown() const;
  bool hasSprite_pImagePrimary() const;
  int sprite_pImagePrimary_anim() const;
  Unit orderTarget_pUnit() const;
  int sprite_pImagePrimary_frameSet() const;
  int shieldPoints() const;
  int energy() const;
  int killCount() const;
  int acidSporeCount() const;
  int defenseMatrixDamage() const;
  int defenseMatrixTimer() const;
  int ensnareTimer() const;
  int irradiateTimer() const;
  int lockdownTimer() const;
  int maelstromTimer() const;
  int plagueTimer() const;
  int removeTimer() const;
  int stasisTimer() const;
  int stimTimer() const;
  int secondaryOrderID() const;
  Unit currentBuildUnit() const;
  Unit moveTarget_pUnit() const;
  Position moveTarget() const;
  Position orderTarget() const;
  Unit building_addon() const;
  Unit nydus_exit() const;
  Unit worker_pPowerup() const;
  int gatherQueueCount() const;
  Unit nextGatherer() const;
  int isBlind() const;
  int resourceType() const;
  int parasiteFlags() const;
  int stormTimer() const;
  int movementState() const;
  int carrier_inHangerCount() const;
  int carrier_outHangerCount() const;
  int vulture_spiderMineCount() const;
  int remainingBuildTime() const;
  bool silo_bReady() const;
  int building_larvaTimer() const;
  int orderQueueTimer() const;
  int building_techType() const;
  int building_upgradeResearchTime() const;
  int building_upgradeType() const;
  Position rally_position() const;
  Unit rally_unit() const;
  int orderState() const;

  void setHitPoints(int value);
  void setShields(int value);
  void setEnergy(int value);
};

struct Bullet {
  bwgame::bullet_t* b = nullptr;
  openbwapi_impl* impl = nullptr;

  explicit operator bool() const;

  size_t getIndex() const;
  bool hasSprite() const;
  Position spritePosition() const;
  Position position() const;
  Unit sourceUnit() const;
  Unit attackTargetUnit() const;
  int type() const;
  int currentDirection() const;
  int current_speed_x() const;
  int current_speed_y() const;
  Position targetPosition() const;
  int time_remaining() const;
};

struct Region {
  size_t index = (size_t)-1;
  openbwapi_impl* impl = nullptr;

  explicit operator bool() const;
  size_t getIndex() const;
  size_t groupIndex() const;
  Position getCenter() const;
  int accessabilityFlags() const;
  int rgnBox_left() const;
  int rgnBox_right() const;
  int rgnBox_top() const;
  int rgnBox_bottom() const;
  size_t neighborCount() const;
  Region getNeighbor(size_t n) const;
};

}
