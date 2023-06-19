#include "Producer.h"

#include <utility>
#include "Strategist.h"
#include "Units.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Workers.h"
#include "UnitUtil.h"

#include <chrono>

#if INSTRUMENTATION_ENABLED
#define DEBUG_WRITE_SUBGOALS false
#define OUTPUT_BUILD_QUEUE false
#endif

namespace Producer
{
#ifndef DEBUG
    namespace
    {
#endif
        const double MINERALS_PER_WORKER_FRAME = 0.0465;
        const double GAS_PER_WORKER_FRAME = 0.071;
        const double MINERALS_PER_GAS_UNIT = 0.655;

        int PREDICT_FRAMES;
        std::vector<int> minerals;
        std::vector<int> gas;
        std::vector<int> supply;
        std::vector<int> totalSupply;
        std::multiset<int> framesWithReassignableMineralWorker;
        int reassignedMineralWorkersAtStartFrame;
        std::map<BuildingPlacement::Neighbourhood, std::map<int, BuildingPlacement::BuildLocationSet>> buildLocations;
        BuildingPlacement::BuildLocationSet availableGeysers;

        const BuildingPlacement::BuildLocation InvalidBuildLocation(Block::Location(BWAPI::TilePositions::Invalid), 0, 0, 0);

        using Type = std::variant<BWAPI::UnitType, UpgradeOrTechType>;

        int buildTime(const Type &type)
        {
            if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                return UnitUtil::BuildTime(*unitType);
            if (auto upgradeOrTechType = std::get_if<UpgradeOrTechType>(&type))
                return upgradeOrTechType->upgradeOrResearchTime();
            return 0;
        }

        struct Producer;

        struct ProductionItem
        {
            struct cmp
            {
                bool operator()(const std::shared_ptr<ProductionItem> &a, const std::shared_ptr<ProductionItem> &b) const
                {
                    return a->startFrame < b->startFrame
                           || (b->startFrame >= a->startFrame && a->completionFrame < b->completionFrame)
                           || (b->startFrame >= a->startFrame && b->completionFrame >= a->completionFrame && a < b);
                }
            };

            // Type, can be a unit or upgrade
            Type type;

            // Location, can be empty, a neighbourhood, or, optionally for buildings, a specific tile position
            ProductionLocation location;

            int mineralPrice() const
            {
                return

                    // Mineral price of the type being produced
                        std::visit([](auto &&arg)
                                   { return arg.mineralPrice(); }, type)

                        // Buildings require a worker to be taken off minerals for a while, so we estimate the impact
                        // In some cases this will be too pessimistic, as we can re-use the same worker for multiple jobs
                        + (int) (2.0 * estimatedWorkerMovementTime * MINERALS_PER_WORKER_FRAME);
            }

            int gasPrice() const
            {
                return std::visit([](auto &&arg)
                                  { return arg.gasPrice(); }, type);
            }

            int supplyProvided() const
            {
                if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                    return unitType->supplyProvided();
                return 0;
            }

            int supplyRequired() const
            {
                if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                    return unitType->supplyRequired();
                return 0;
            }

            bool is(BWAPI::UnitType unitType) const
            {
                auto _unitType = std::get_if<BWAPI::UnitType>(&type);
                return _unitType && *_unitType == unitType;
            }

            mutable int startFrame;
            mutable int completionFrame;

            // Fields applicable to non-buildings
            const std::shared_ptr<Producer> producer;

            // Fields applicable to buildings
            bool isPrerequisite;
            Building *queuedBuilding;
            mutable BuildingPlacement::BuildLocation buildLocation;
            mutable MyUnit reservedBuilder;
            mutable int estimatedWorkerMovementTime;

            ProductionItem(const Type &type,
                           int startFrame,
                           ProductionLocation location = std::monostate(),
                           std::shared_ptr<Producer> producer = nullptr,
                           bool isPrerequisite = false)
                    : type(type)
                    , location(std::move(location))
                    , startFrame(startFrame)
                    , completionFrame(startFrame + buildTime(type))
                    , producer(std::move(producer))
                    , isPrerequisite(isPrerequisite)
                    , queuedBuilding(nullptr)
                    , buildLocation(InvalidBuildLocation)
                    , reservedBuilder(nullptr)
                    , estimatedWorkerMovementTime(0) {}

            // Constructor for pending buildings
            explicit ProductionItem(Building *queuedBuilding)
                    : type(queuedBuilding->type)
                    , location(std::monostate())
                    , startFrame(queuedBuilding->expectedFramesUntilStarted())
                    , completionFrame(queuedBuilding->expectedFramesUntilCompletion())
                    , producer(nullptr)
                    , isPrerequisite(false)
                    , queuedBuilding(queuedBuilding)
                    , buildLocation(BuildingPlacement::BuildLocation(Block::Location(queuedBuilding->tile), 0, 0, 0))
                    , reservedBuilder(nullptr)
                    , estimatedWorkerMovementTime(0) {}
        };

        typedef std::multiset<std::shared_ptr<ProductionItem>, ProductionItem::cmp> ProductionItemSet;

        struct Producer
        {
            int availableFrom;
            std::shared_ptr<ProductionItem> queued;
            MyUnit existing;
            ProductionItemSet items;

            Producer(std::shared_ptr<ProductionItem> queued, int availableFrom)
                    : availableFrom(availableFrom)
                    , queued(std::move(queued))
                    , existing(nullptr) {}

            Producer(MyUnit existing, int availableFrom)
                    : availableFrom(availableFrom)
                    , queued(nullptr)
                    , existing(std::move(existing)) {}
        };

        std::map<std::shared_ptr<ProductionItem>, std::shared_ptr<Producer>> queuedProducers;
        std::map<MyUnit, std::shared_ptr<Producer>> existingProducers;

        ProductionItemSet committedItems;

#if OUTPUT_BUILD_QUEUE
        void write(ProductionItemSet &items, const std::string &label)
        {
            auto itemLabel = [](const std::shared_ptr<ProductionItem> &item)
            {
                std::ostringstream labelstream;
                std::visit([&labelstream](auto &&arg)
                           { labelstream << arg; }, item->type);
                if (item->queuedBuilding && item->queuedBuilding->isConstructionStarted()) labelstream << "^";
                if (item->queuedBuilding && !item->queuedBuilding->isConstructionStarted()) labelstream << "*";
                if (item->buildLocation.location.tile.isValid())
                    labelstream << "(" << item->buildLocation.location.tile.x << ":" << item->buildLocation.location.tile.y << ")";
                return labelstream.str();
            };

            std::vector<std::string> values;
            for (auto &item : items)
            {
                std::ostringstream value;
                value << item->startFrame << ": " << itemLabel(item) << " (" << item->completionFrame << ")";
                values.push_back(value.str());
            }

            CherryVis::setBoardListValue(label, values);

            /*
            Log::LogWrapper csv = Log::Csv(label);

            int seconds = currentFrame / 24;
            csv << currentFrame;
            csv << (seconds / 60);
            csv << (seconds % 60);
            csv << BWAPI::Broodwar->self()->minerals();
            csv << BWAPI::Broodwar->self()->gas();
            csv << (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());

            if (currentFrame < 2) Log::Debug() << label << ":";

            for (auto &item : items)
            {
                if (item->startFrame >= PREDICT_FRAMES) continue;
                csv << itemLabel(item) << item->startFrame << item->completionFrame;
                csv << minerals[item->startFrame] << gas[item->startFrame] << supply[item->startFrame];

                if (currentFrame < 2) Log::Debug() << itemLabel(item) << ": " << item->startFrame << ";" << item->completionFrame;
            }
             */
        }
#endif

        int getRemainingBuildTime(const MyUnit &producer)
        {
            if (producer->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Train &&
                (currentFrame - producer->lastCommandFrame - 1) <= BWAPI::Broodwar->getLatencyFrames())
            {
                return buildTime(producer->bwapiUnit->getLastCommand().getUnitType());
            }

            if (!producer->bwapiUnit->isTraining()) return -1;

            return producer->bwapiUnit->getRemainingTrainTime();
        }

        int getRemainingUpgradeTime(const MyUnit &producer)
        {
            if (producer->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Upgrade &&
                (currentFrame - producer->lastCommandFrame - 1) <= BWAPI::Broodwar->getLatencyFrames())
            {
                return buildTime(producer->bwapiUnit->getLastCommand().getUpgradeType());
            }

            if (!producer->bwapiUnit->isUpgrading()) return -1;

            return producer->bwapiUnit->getRemainingUpgradeTime();
        }

        int getRemainingResearchTime(const MyUnit &producer)
        {
            if (producer->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Research &&
                (currentFrame - producer->lastCommandFrame - 1) <= BWAPI::Broodwar->getLatencyFrames())
            {
                return buildTime(producer->bwapiUnit->getLastCommand().getTechType());
            }

            if (!producer->bwapiUnit->isResearching()) return -1;

            return producer->bwapiUnit->getRemainingResearchTime();
        }

        void updateResourceCollection(std::vector<int> &resource, int fromFrame, int workerChange, double baseRate)
        {
            double delta = (double) workerChange * baseRate;
            for (int f = fromFrame + 1; f < PREDICT_FRAMES; f++)
            {
                resource[f] += (int) (delta * (f - fromFrame));
            }
        }

        void spendResource(std::vector<int> &resource, int amount, int fromFrame, int toFrame = PREDICT_FRAMES)
        {
            for (int f = fromFrame; f < toFrame; f++)
            {
                resource[f] -= amount;
            }
        }

        void moveResourceSpend(std::vector<int> &resource, int amount, int fromFrame, int delta)
        {
            // If the delta is positive, add back the resource into the future, otherwise subtract it into the past
            if (delta > 0)
            {
                spendResource(resource, -amount, fromFrame, fromFrame + delta);
            }
            else
            {
                spendResource(resource, amount, fromFrame + delta, fromFrame);
            }
        }

        void addProvidedSupply(int amount, int fromFrame, int toFrame = PREDICT_FRAMES)
        {
            // Start by adding the supply to the total
            spendResource(totalSupply, -amount, fromFrame, toFrame);

            // If the final supply does not exceed the max, we can just add the provided supply
            // We only need to check the last frame, as total supply only increases over time
            if (totalSupply[toFrame - 1] <= 400)
            {
                spendResource(supply, -amount, fromFrame, toFrame);
                return;
            }

            // Add provided supply up to max supply
            for (int f = fromFrame; f < toFrame; f++)
            {
                // We can short-circuit if at any point we were already maxed before this provider was added
                if (totalSupply[f] >= 400 + amount) return;

                supply[f] += 400 + amount - totalSupply[f];
            }
        }

        void moveProvidedSupply(int amount, int fromFrame, int delta)
        {
            // Moving back is the same as adding supply in the changed frames
            if (delta < 0)
            {
                addProvidedSupply(amount, fromFrame + delta, fromFrame);
                return;
            }

            // Moving forward requires us to adjust whenever we capped for max supply earlier

            // Start by removing the total supply
            spendResource(totalSupply, amount, fromFrame, fromFrame + delta);

            // Now loop backwards through the available supply
            for (int f = fromFrame + delta - 1; f >= fromFrame; f--)
            {
                // If we were not maxed here, the rest of the frames were not capped and can be handled directly
                if (totalSupply[f] <= 400 - amount)
                {
                    spendResource(supply, amount, fromFrame, f + 1);
                    return;
                }

                // If we were already maxed here without this provider, move to the next frame
                if (totalSupply[f] >= 400) continue;

                // Remove the supply, respecting the cap
                supply[f] -= 400 - totalSupply[f];
            }
        }

        void initializeResources()
        {
            // Decide on the length of the prediction window
            // In the early game we use a 4500-frame window, which shrinks to 2000 later on
            PREDICT_FRAMES = std::max(2000, std::min(4500, 12000 - currentFrame));

            // Fill mineral and gas with current rates
            minerals.assign(PREDICT_FRAMES, BWAPI::Broodwar->self()->minerals());
            gas.assign(PREDICT_FRAMES, BWAPI::Broodwar->self()->gas());
            double mineralRate = (double) Workers::mineralWorkers() * MINERALS_PER_WORKER_FRAME;
            auto gasWorkers = Workers::gasWorkers();
            double gasRate = ((double) gasWorkers.first * GAS_PER_WORKER_FRAME) + ((double) gasWorkers.second * GAS_PER_WORKER_FRAME / 4.0);
            for (int f = 0; f < PREDICT_FRAMES; f++)
            {
                minerals[f] += (int) (mineralRate * f);
                gas[f] += (int) (gasRate * f);
            }

            // Fill supply with available and provided supply
            // For provided we use the total of our completed supply providers as we want to store values over the supply limit
            supply.assign(PREDICT_FRAMES, BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());
            totalSupply.assign(PREDICT_FRAMES,
                               Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) * BWAPI::UnitTypes::Protoss_Nexus.supplyProvided()
                               + Units::countCompleted(BWAPI::UnitTypes::Protoss_Pylon) * BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());

            // Assume workers being built will go to minerals
            for (const auto &unit : Units::allMine())
            {
                if (!unit->completed) continue;
                if (!unit->type.isResourceDepot()) continue;

                int remainingTrainTime = getRemainingBuildTime(unit);
                if (remainingTrainTime >= 0) updateResourceCollection(minerals, remainingTrainTime + 1, 1, MINERALS_PER_WORKER_FRAME);
            }

            // Initialize workers available for transfer
            framesWithReassignableMineralWorker.clear();
            reassignedMineralWorkersAtStartFrame = 0;
            int reassignableMineralWorkersNow = Workers::reassignableMineralWorkers();
            for (int i=0; i<reassignableMineralWorkersNow; i++)
            {
                framesWithReassignableMineralWorker.insert(0);
            }

            // Adjust for pending buildings
            // This also adds all pending buildings to the committed item set
            // TODO: Should be able to change our minds and cancel pending buildings that are no longer needed
            for (const auto &pendingBuilding : Builder::allPendingBuildings())
            {
                // "Commit" the item and spend resources if the building isn't started
                auto item = *committedItems.emplace(std::make_shared<ProductionItem>(pendingBuilding.get()));
                if (!pendingBuilding->isConstructionStarted())
                {
                    spendResource(minerals, pendingBuilding->type.mineralPrice(), item->startFrame);
                    spendResource(gas, pendingBuilding->type.gasPrice(), item->startFrame);
                }

                // Supply providers
                if (pendingBuilding->type.supplyProvided() > 0)
                {
                    addProvidedSupply(pendingBuilding->type.supplyProvided(), item->completionFrame);
                }

                // Refineries are assumed to allow three workers to transfer on completion
                // In certain cases this might not be possible, but for simplicity we assume there will always be three workers available
                if (pendingBuilding->type == BWAPI::Broodwar->self()->getRace().getRefinery())
                {
                    framesWithReassignableMineralWorker.insert(item->completionFrame);
                    framesWithReassignableMineralWorker.insert(item->completionFrame);
                    framesWithReassignableMineralWorker.insert(item->completionFrame);
                }
            }

            // Handle mineral reservations
            for (const auto &mineralReservation : Strategist::currentMineralReservations())
            {
                int amount = mineralReservation.first;
                int desiredFrame = mineralReservation.second;

                // Scan backwards to find the earliest frame the item can be produced after the desired frame
                int frame;
                for (frame = PREDICT_FRAMES - 1; frame >= desiredFrame; frame--)
                {
                    if (minerals[frame] < amount) break;
                }
                frame++;

                // Abort now if we never have enough minerals
                if (frame >= PREDICT_FRAMES) continue;

                // Spend the minerals at the detected frame
                spendResource(minerals, amount, frame);
            }

            buildLocations = BuildingPlacement::getBuildLocations();
            availableGeysers = BuildingPlacement::availableGeysers();
            queuedProducers.clear();
            existingProducers.clear();
        }

        // Need a forward reference since the next two methods do mutual recursion
        void addMissingPrerequisites(ProductionItemSet &items,
                                     BWAPI::UnitType unitType,
                                     const ProductionLocation &location,
                                     BWAPI::UnitType producerType,
                                     int startFrame = 0);

        void addBuildingIfIncomplete(ProductionItemSet &items,
                                     BWAPI::UnitType unitType,
                                     const ProductionLocation &location,
                                     BWAPI::UnitType producerType,
                                     int baseFrame = 0,
                                     bool isPrerequisite = false)
        {
            if (Units::countCompleted(unitType) > 0) return;

            // If we are already building one, add an item as a placeholder for how much longer it will take to complete it
            // This will ensure any later prerequisites are scheduled correctly
            // The placeholder item will be removed when resolving duplicates
            auto pendingBuildings = Builder::pendingBuildingsOfType(unitType);
            if (!pendingBuildings.empty())
            {
                int timeToCompletion = INT_MAX;
                for (auto &pendingBuilding : pendingBuildings)
                {
                    timeToCompletion = std::min(timeToCompletion, pendingBuilding->expectedFramesUntilCompletion());
                }

                items.emplace(std::make_shared<ProductionItem>(unitType, baseFrame - timeToCompletion, std::monostate(), nullptr, isPrerequisite));
                return;
            }

            // If this item is the producer of our target unit, make sure it is in the correct location
            // Otherwise it doesn't matter
            ProductionLocation itemLocation = unitType == producerType ? location : std::monostate();

            int startFrame = baseFrame - UnitUtil::BuildTime(unitType);
            items.emplace(std::make_shared<ProductionItem>(unitType, startFrame, itemLocation, nullptr, isPrerequisite));
            addMissingPrerequisites(items, unitType, location, producerType, startFrame);
        }

        // For the specified unit type, recursively checks what prerequisites are missing and adds them to the set
        // The frames are relative to the time when the first unit of the desired type can be produced
        void addMissingPrerequisites(ProductionItemSet &items,
                                     BWAPI::UnitType unitType,
                                     const ProductionLocation &location,
                                     BWAPI::UnitType producerType,
                                     int startFrame)
        {
            for (auto typeAndCount : unitType.requiredUnits())
            {
                // Don't include workers
                if (typeAndCount.first.isWorker()) continue;

                addBuildingIfIncomplete(items, typeAndCount.first, location, producerType, startFrame, true);
            }
        }

        // For buildings, whether this building can produce an item at the given location
        bool canProduceFrom(const Type &type, const ProductionLocation &location, const ProductionItem &producerItem)
        {
            // Handle case where we want to produce a worker from a specific base
            auto base = std::get_if<Base *>(&location);
            if (base)
            {
                auto unitType = std::get_if<BWAPI::UnitType>(&type);
                if (!unitType || !unitType->isWorker()) return true;

                return producerItem.buildLocation.location.tile == (*base)->getTilePosition();
            }

            // Handle case where we want to produce from a specific neighbourhood
            auto neighbourhood = std::get_if<BuildingPlacement::Neighbourhood>(&location);
            if (neighbourhood)
            {
                return BuildingPlacement::isInNeighbourhood(producerItem.buildLocation.location.tile, *neighbourhood);
            }

            return true;
        }

        // For buildings, whether this building can produce an item at the given location
        bool canProduceFrom(const Type &type, const ProductionLocation &location, const MyUnit &producer)
        {
            // Handle case where we want to produce a worker from a specific base
            auto base = std::get_if<Base *>(&location);
            if (base)
            {
                auto unitType = std::get_if<BWAPI::UnitType>(&type);
                if (!unitType || !unitType->isWorker()) return true;

                return producer == (*base)->resourceDepot;
            }

            // Handle case where we want to produce from a specific neighbourhood
            auto neighbourhood = std::get_if<BuildingPlacement::Neighbourhood>(&location);
            if (neighbourhood)
            {
                return BuildingPlacement::isInNeighbourhood(producer->buildTile, *neighbourhood);
            }

            return true;
        }

        // Shifts one item in the given set by the given amount
        void shiftOne(ProductionItemSet &items, std::shared_ptr<ProductionItem> item, int delta)
        {
            if (delta == 0) return;

            // Adjust resources

            if (item->supplyProvided() > 0)
            {
                moveProvidedSupply(item->supplyProvided(), item->completionFrame, delta);
            }

            if (item->gasPrice() > 0)
            {
                moveResourceSpend(gas, item->gasPrice(), item->startFrame, delta);
            }

            moveResourceSpend(minerals, item->mineralPrice(), item->startFrame, delta);

            // We assume a refinery causes three workers to move from minerals to gas
            if (item->is(BWAPI::Broodwar->self()->getRace().getRefinery()))
            {
                updateResourceCollection(minerals, item->completionFrame, 3, MINERALS_PER_WORKER_FRAME);
                updateResourceCollection(gas, item->completionFrame, -3, GAS_PER_WORKER_FRAME);
                updateResourceCollection(minerals, item->completionFrame + delta, -3, MINERALS_PER_WORKER_FRAME);
                updateResourceCollection(gas, item->completionFrame + delta, 3, GAS_PER_WORKER_FRAME);
            }

            // We keep the same item, but reinsert it to ensure the set is correctly ordered
            items.erase(item);
            item->startFrame += delta;
            item->completionFrame += delta;
            items.insert(item);
        }

        // Shifts the item pointed to by the iterator and all future items by the given number of frames
        void shiftAll(ProductionItemSet &items, ProductionItemSet::iterator it, int delta)
        {
            if (delta < 1 || it == items.end()) return;

            for (; it != items.end(); it++)
            {
                auto &item = **it;

                item.startFrame += delta;
                item.completionFrame += delta;
            }
        }

        // Remove duplicate items from this goal's set and resolve conflicts with already-committed items
        // Returns the max frame an item in goalItems will finish
        int resolveDuplicates(ProductionItemSet &goalItems)
        {
            int maxFrame = 0;
            std::set<BWAPI::UnitType> seen;
            for (auto it = goalItems.begin(); it != goalItems.end();)
            {
                auto &item = **it;

                auto unitType = std::get_if<BWAPI::UnitType>(&item.type);
                if (!unitType) continue;

                // There's already an item of this type in this goal's queue
                if (seen.find(*unitType) != seen.end())
                {
                    it = goalItems.erase(it);
                    continue;
                }

                // Check if there is a matching item in the committed item set
                bool handled = false;
                for (auto &committedItem : committedItems)
                {
                    auto committedUnitType = std::get_if<BWAPI::UnitType>(&committedItem->type);
                    if (!committedUnitType || *unitType != *committedUnitType) continue;

                    // We found a match

                    // Compute the difference in completion times
                    int delta = committedItem->completionFrame - item.completionFrame;

                    // Remove the item
                    it = goalItems.erase(it);

                    // If the committed item completes later, shift all the later items in this goal's queue
                    // The rationale for this is that the committed item set is already optimized to produce everything
                    // as early as possible, so we know we can't move it earlier
                    shiftAll(goalItems, it, delta);

                    maxFrame = std::max(maxFrame, committedItem->completionFrame);
                    handled = true;
                    break;
                }

                // We didn't find a match, so just register this type and continue
                if (!handled)
                {
                    maxFrame = std::max(maxFrame, (*it)->completionFrame);
                    seen.insert(*unitType);
                    it++;
                }
            }

            return maxFrame;
        }

        void choosePylonBuildLocation(const ProductionItem &pylon, bool tentative = false, int requiredWidth = 0)
        {
            if (pylon.buildLocation.location.tile.isValid()) return;

            auto fixedNeighbourhood = std::get_if<BuildingPlacement::Neighbourhood>(&pylon.location);
            auto neighbourhood = fixedNeighbourhood ? *fixedNeighbourhood : BuildingPlacement::Neighbourhood::AllMyBases;

            auto &pylonLocations = buildLocations[neighbourhood][2];
            if (pylonLocations.empty()) return;

            // All else being equal, try to keep two of each location type powered
            int desiredMedium = std::max(0, 2 - (int) buildLocations[neighbourhood][3].size());
            int desiredLarge = std::max(0, 2 - (int) buildLocations[neighbourhood][4].size());

            // Find the first pylon that meets the requirements

            // Score the locations based on distance and what they will power
            int bestScore = INT_MAX;
            auto best = pylonLocations.end();
            for (auto it = pylonLocations.begin(); it != pylonLocations.end(); it++)
            {
                auto &pylonLocation = *it;

                // If we have a hard requirement, don't consider anything that doesn't satisfy it
                if ((requiredWidth == 3 && pylonLocation.powersMedium.empty()) ||
                    (requiredWidth == 4 && pylonLocation.powersLarge.empty()))
                {
                    continue;
                }

                int score = std::max(0, desiredMedium - (int) pylonLocation.powersMedium.size())
                            + std::max(0, desiredLarge - (int) pylonLocation.powersLarge.size());
                if (score < bestScore)
                {
                    bestScore = score;
                    best = it;

                    if (bestScore == 0) break;
                }
            }

            // If we found a match, use it
            if (best != pylonLocations.end())
            {
                pylon.estimatedWorkerMovementTime = best->builderFrames;
                if (!tentative)
                {
                    pylon.buildLocation = *best;
                    pylonLocations.erase(best);
                }
                return;
            }

            // We couldn't satisfy our hard requirement, so just return the first one
            pylon.estimatedWorkerMovementTime = pylonLocations.begin()->builderFrames;
            if (!tentative)
            {
                pylon.buildLocation = *pylonLocations.begin();
                pylonLocations.erase(pylonLocations.begin());
            }
        }

        void reserveBuildPositions(ProductionItemSet &items)
        {
            for (auto it = items.begin(); it != items.end(); it++)
            {
                auto &item = **it;

                if (item.queuedBuilding) continue;
                if (item.buildLocation.location.tile.isValid()) continue;

                // Choose a tentative build location for pylons that don't already have one
                // It may be made concrete later to account for psi requirements
                if (item.is(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    choosePylonBuildLocation(item, true);
                    continue;
                }

                auto unitType = std::get_if<BWAPI::UnitType>(&item.type);
                if (!unitType) continue;
                if (!unitType->isBuilding()) continue;
                if (!unitType->requiresPsi()) continue;

                auto locationNeighbourhood = std::get_if<BuildingPlacement::Neighbourhood>(&item.location);
                auto neighbourhood = locationNeighbourhood ? *locationNeighbourhood : BuildingPlacement::Neighbourhood::AllMyBases;
                auto &locations = buildLocations[neighbourhood][unitType->tileWidth()];

                // Get the frame when the next available build location will be powered
                int availableAt = INT_MAX;
                auto location = locations.begin();
                for (; location != locations.end(); location++)
                {
                    if (*unitType != BWAPI::UnitTypes::Protoss_Robotics_Facility || location->location.hasExit)
                    {
                        availableAt = location->framesUntilPowered;
                        break;
                    }
                }

                // If there is an available location now, just take it
                if (availableAt <= item.startFrame)
                {
                    // If we are building a Stargate, peek ahead to see if there is a location with lower builder frames
                    // Here the "exit" is not really a concern
                    if (*unitType == BWAPI::UnitTypes::Protoss_Stargate)
                    {
                        auto alternateLocation = locations.begin();
                        std::advance(alternateLocation, 1);
                        for (; alternateLocation != locations.end(); alternateLocation++)
                        {
                            if (alternateLocation->framesUntilPowered > availableAt) break;
                            if (alternateLocation->builderFrames < location->builderFrames)
                            {
                                location = alternateLocation;
                                availableAt = location->framesUntilPowered;
                            }
                        }
                    }

                    item.estimatedWorkerMovementTime = location->builderFrames;
                    item.buildLocation = *location;
                    locations.erase(location);
                    continue;
                }

                // Find a committed pylon that has no assigned build location
                std::shared_ptr<ProductionItem> pylon = nullptr;
                for (auto &committedItem : committedItems)
                {
                    if (!committedItem->is(BWAPI::UnitTypes::Protoss_Pylon)) continue;
                    if (committedItem->queuedBuilding) continue;
                    if (committedItem->buildLocation.location.tile.isValid()) continue;

                    pylon = committedItem;
                    break;
                }

                // If there isn't one, queue it unless we can't beat our current availability frame
                if (!pylon && availableAt >= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    int startFrame = std::max(0, item.startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                    pylon = *items.emplace(std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Pylon, startFrame, item.location));
                }

                // If the pylon will complete later than we need it, try to move it earlier
                if (pylon && pylon->completionFrame > item.startFrame && pylon->startFrame > 0)
                {
                    // Try to shift the pylon so that it completes right when we need it
                    int desiredStartFrame = std::max(0, item.startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));

                    // Determine how far we can move the pylon back and still have minerals
                    int mineralFrame;
                    for (mineralFrame = pylon->startFrame - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                    {
                        if (minerals[mineralFrame] < BWAPI::UnitTypes::Protoss_Pylon.mineralPrice()) break;
                    }
                    mineralFrame++;

                    // If the new completion frame is worse than the current best, don't bother
                    if (mineralFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) >= availableAt)
                    {
                        pylon = nullptr;
                    }

                        // Otherwise, if we could move it back, make the relevant adjustments
                    else
                    {
                        shiftOne(committedItems, pylon, mineralFrame - pylon->startFrame);
                    }
                }

                // If we are creating or moving a pylon, pick a location and add the new build locations
                if (pylon)
                {
                    choosePylonBuildLocation(*pylon, false, unitType->tileWidth());

                    // TODO: The powered after times are wrong here, as the pylon may get moved later
                    // Should perhaps look for available build locations within the committed pylon set instead
                    // Might make sense to move the build location stuff after minerals, etc. too
                    for (auto &poweredBuildLocation : pylon->buildLocation.powersMedium)
                    {
                        buildLocations[neighbourhood][3].emplace(poweredBuildLocation.location,
                                                                 poweredBuildLocation.builderFrames,
                                                                 pylon->completionFrame,
                                                                 poweredBuildLocation.distanceToExit);
                    }
                    for (auto &poweredBuildLocation : pylon->buildLocation.powersLarge)
                    {
                        buildLocations[neighbourhood][4].emplace(poweredBuildLocation.location,
                                                                 poweredBuildLocation.builderFrames,
                                                                 pylon->completionFrame,
                                                                 poweredBuildLocation.distanceToExit);
                    }
                }

                location = locations.begin();
                for (; location != locations.end(); location++)
                {
                    if (*unitType != BWAPI::UnitTypes::Protoss_Robotics_Facility || location->location.hasExit)
                    {
                        break;
                    }
                }

                // TODO: Cancel everything if there are no build locations
                if (location == locations.end()) return;

                // Take the best build location
                item.estimatedWorkerMovementTime = location->builderFrames;
                item.buildLocation = *location;
                locations.erase(location);

                // If it is first powered later, shift the remaining items in the queue
                shiftAll(items, it, item.buildLocation.framesUntilPowered - item.startFrame);
            }
        }

        // Gets the frame at which a producer is available to build something
        int availableAt(const Type &type, int startFrame, const std::shared_ptr<Producer> &producer)
        {
            int build = buildTime(type);
            auto it = producer->items.begin();

            int frame = std::max(startFrame, producer->availableFrom);
            while (frame < PREDICT_FRAMES)
            {
                // If the producer already has queued items, make sure this item can be inserted
                // Otherwise advance the frame to when the item is completed
                if (it != producer->items.end() && frame >= ((*it)->startFrame - build))
                {
                    frame = std::max(frame, (*it)->completionFrame);
                    it++;
                    continue;
                }

                // The producer is available at this frame
                return frame;
            }

            return PREDICT_FRAMES;
        }

        // When prerequisites are involved, the start frames will initially be negative
        // This shifts all of the frames in the set forward so they start at 0
        void normalizeStartFrame(ProductionItemSet &items)
        {
            int offset = 0;
            bool first = true;
            for (auto &item : items)
            {
                if (first) offset = -item->startFrame, first = false;
                item->startFrame += offset;
                item->completionFrame += offset;
            }
        }

        // Finds the frame where we have enough of a resource for the item and its prerequisites
        int frameWhenResourcesMet(const ProductionItem &item, ProductionItemSet &prerequisiteItems, bool isMinerals)
        {
            auto &resource = isMinerals ? minerals : gas;
            int itemCost = isMinerals ? item.mineralPrice() : item.gasPrice();

            int f;

            // Simple case: no prerequisites
            if (prerequisiteItems.empty())
            {
                // Find the first frame scanning backwards where we don't have enough of the resource, then advance one
                for (f = PREDICT_FRAMES - 1; f >= item.startFrame; f--)
                {
                    if (resource[f] < itemCost) break;
                }

                return f + 1;
            }

            // If we haven't taken gas yet and the prerequisite items list includes a cybernetics core, we need to include
            // the mineral cost of the gas (i.e. minerals lost by shifting workers to mine gas)
            bool includeGasCostInMinerals = false;
            if (isMinerals && !availableGeysers.empty() && Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0)
            {
                for (auto &prerequisite : prerequisiteItems)
                {
                    if (prerequisite->isPrerequisite && prerequisite->is(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
                    {
                        includeGasCostInMinerals = true;
                        break;
                    }
                }
            }

            // Get the total resource cost of the prerequisites
            int prerequisiteCost = 0;
            for (auto &prerequisiteItem : prerequisiteItems)
            {
                prerequisiteCost += isMinerals ? prerequisiteItem->mineralPrice() : prerequisiteItem->gasPrice();

                if (includeGasCostInMinerals)
                {
                    prerequisiteCost += (int) (MINERALS_PER_GAS_UNIT * (double) prerequisiteItem->gasPrice());

                    // Include the price of the assimilator with the cybernetics core
                    if (prerequisiteItem->isPrerequisite && prerequisiteItem->is(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
                    {
                        prerequisiteCost += BWAPI::UnitTypes::Protoss_Assimilator.mineralPrice();
                    }
                }
            }

            // Precompute the frame stops and how much of the resource we need at each one
            std::vector<std::pair<int, int>> frameStopsAndResourceNeeded;
            frameStopsAndResourceNeeded.emplace_back(0, prerequisiteCost + itemCost);
            for (auto prerequisiteIt = prerequisiteItems.rbegin(); prerequisiteIt != prerequisiteItems.rend(); prerequisiteIt++)
            {
                int cost = isMinerals ? (*prerequisiteIt)->mineralPrice() : (*prerequisiteIt)->gasPrice();
                if (cost > 0)
                {
                    frameStopsAndResourceNeeded.emplace_back(item.startFrame - (*prerequisiteIt)->startFrame, prerequisiteCost);
                    prerequisiteCost -= cost;

                    if (includeGasCostInMinerals)
                    {
                        prerequisiteCost -= (int) (MINERALS_PER_GAS_UNIT * (double) (*prerequisiteIt)->gasPrice());

                        // Include the price of the assimilator with the cybernetics core
                        if ((*prerequisiteIt)->isPrerequisite && (*prerequisiteIt)->is(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
                        {
                            prerequisiteCost -= BWAPI::UnitTypes::Protoss_Assimilator.mineralPrice();
                        }
                    }
                }
            }

            // Scan backwards, breaking whenever we don't have enough minerals at any of the stops
            for (f = PREDICT_FRAMES - 1; f >= item.startFrame; f--)
            {
                for (auto &frameStopAndMineralsNeeded : frameStopsAndResourceNeeded)
                {
                    int frame = f - frameStopAndMineralsNeeded.first;
                    if (frame >= PREDICT_FRAMES) continue;
                    if (frame < 0 || resource[frame] < frameStopAndMineralsNeeded.second) return f + 1;
                }
            }

            return f + 1;
        }

        bool shiftForMinerals(ProductionItem &item, ProductionItemSet &prerequisiteItems)
        {
            int mineralCost = item.mineralPrice();

            // Find the frame where we have enough minerals from that point forward
            int f = frameWhenResourcesMet(item, prerequisiteItems, true);

            // Workers are a special case, as they start producing income after they are completed
            // So for them we just need enough minerals to cover the period until they have recovered their investment
            if (f > item.startFrame && item.is(BWAPI::UnitTypes::Protoss_Probe))
            {
                f = item.startFrame;
                int timeToBuild = buildTime(item.type);
                for (int i = item.startFrame; i < PREDICT_FRAMES; i++)
                {
                    int requiredMinerals = mineralCost;
                    int miningTime = i - f - timeToBuild;
                    if (miningTime > 0) requiredMinerals -= (int) ((double) miningTime * MINERALS_PER_WORKER_FRAME);
                    if (requiredMinerals <= 0) break;
                    if (minerals[i] < requiredMinerals) f = i;
                }
            }

            // If we can't ever produce this item, return false
            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            // If we can't produce the item immediately, shift the start frame for this and all remaining items
            // Since we are preserving the relative order, we don't need to reinsert anything into the set
            int delta = f - item.startFrame;
            if (delta > 0)
            {
                item.startFrame += delta;
                item.completionFrame += delta;
                shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);
            }
            return true;
        }

        bool shiftForGas(ProductionItem &item, ProductionItemSet &prerequisiteItems, bool commit)
        {
            // Before resolving a gas block, check if we have a cybernetics core as a prerequisite
            // If so, insert an assimilator immediately at the same time unless we already have one
            if (!availableGeysers.empty() && Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0)
            {
                for (auto &prerequisiteItem : prerequisiteItems)
                {
                    if (prerequisiteItem->isPrerequisite && prerequisiteItem->is(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
                    {
                        // Spend minerals
                        spendResource(minerals, BWAPI::UnitTypes::Protoss_Assimilator.mineralPrice(), prerequisiteItem->startFrame);

                        // Update mineral and gas collection
                        int completionFrame = prerequisiteItem->startFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Assimilator);
                        framesWithReassignableMineralWorker.insert(completionFrame);
                        framesWithReassignableMineralWorker.insert(completionFrame);
                        framesWithReassignableMineralWorker.insert(completionFrame);

                        // Commit
                        auto geyser = *committedItems.emplace(std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Assimilator,
                                                                                               prerequisiteItem->startFrame));
                        geyser->buildLocation = *availableGeysers.begin();
                        availableGeysers.erase(availableGeysers.begin());

                        // There will be only one
                        break;
                    }
                }
            }

            // Get the total gas cost of the prerequisites
            int prerequisiteCost = 0;
            for (auto &prerequisiteItem : prerequisiteItems)
            {
                prerequisiteCost += prerequisiteItem->gasPrice();
            }

            // Jump out now if the item doesn't require gas
            if (item.gasPrice() == 0 && prerequisiteCost == 0) return true;

            // Precompute the frame stops and how much gas we need at each one
            int prerequisiteCostRemaining = prerequisiteCost;
            std::vector<std::pair<int, int>> frameStopsAndGasNeeded;
            frameStopsAndGasNeeded.emplace_back(0, prerequisiteCostRemaining + item.gasPrice());
            for (auto prerequisiteIt = prerequisiteItems.rbegin(); prerequisiteIt != prerequisiteItems.rend(); prerequisiteIt++)
            {
                int cost = (*prerequisiteIt)->gasPrice();
                if (cost > 0)
                {
                    frameStopsAndGasNeeded.emplace_back(item.startFrame - (*prerequisiteIt)->startFrame, prerequisiteCostRemaining);
                    prerequisiteCostRemaining -= cost;
                }
            }

            // While we have workers we can assign to gas, repeatedly assign one of them to resolve any gas blocks
            while (!framesWithReassignableMineralWorker.empty())
            {
                // Find the earliest gas block
                auto getGasBlockFrame = [&frameStopsAndGasNeeded, &item]()
                {
                    for (int f = item.startFrame; f < PREDICT_FRAMES; f++)
                    {
                        for (auto &frameStopAndGasNeeded : frameStopsAndGasNeeded)
                        {
                            int frame = f - frameStopAndGasNeeded.first;
                            if (frame >= PREDICT_FRAMES) continue;
                            if (frame < 0 || gas[frame] < frameStopAndGasNeeded.second) return f + 1;
                        }
                    }
                    return PREDICT_FRAMES;
                };
                int gasBlockFrame = getGasBlockFrame();

                // If there is no gas block, return immediately
                if (gasBlockFrame == PREDICT_FRAMES) return true;

                // Get the first frame where we have a worker we can reassign
                int reassignFrame = *framesWithReassignableMineralWorker.begin();

                // Find the first frame where we have enough minerals to reassign it
                int mineralBlockFrame;
                for (mineralBlockFrame = PREDICT_FRAMES - 1; mineralBlockFrame > reassignFrame; mineralBlockFrame--)
                {
                    int mineralsNeeded = (int) ((mineralBlockFrame - reassignFrame) * MINERALS_PER_WORKER_FRAME);
                    if (minerals[mineralBlockFrame] < mineralsNeeded) break;
                }

                // If we never have enough minerals, abort here, as we can't produce the item
                if (mineralBlockFrame == (PREDICT_FRAMES - 1)) return false;

                // If this is too late to help resolve the gas block, shift the item
                int delta = mineralBlockFrame - gasBlockFrame;
                if (delta > 0)
                {
                    item.startFrame += delta;
                    item.completionFrame += delta;
                    shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);
                }

                // Move the worker from minerals to gas
                // Note that this does not take refineries into account that require 4 workers - we will overestimate gas collection slightly
                updateResourceCollection(minerals, mineralBlockFrame, -1, MINERALS_PER_WORKER_FRAME);
                updateResourceCollection(gas, mineralBlockFrame, 1, GAS_PER_WORKER_FRAME);

                // If it is frame 0, record this
                if (mineralBlockFrame == 0)
                {
                    reassignedMineralWorkersAtStartFrame++;
                }

                // Remove the reassignable worker
                framesWithReassignableMineralWorker.erase(framesWithReassignableMineralWorker.begin());
            }

            // Determine the frame where we need 3 additional gas workers to resolve a gas deficit
            int gasCost = item.gasPrice();
            int gasDeficit = 0;
            int gasDeficitFrame = PREDICT_FRAMES;

            // Simple case: no prerequisites, or the prerequisites require no gas
            if (prerequisiteCost == 0)
            {
                if (gasCost == 0) return true;
                for (int f = PREDICT_FRAMES - 1; f >= item.startFrame; f--)
                {
                    int deficit = gasCost - gas[f];
                    if (deficit > 0)
                    {
                        int deficitFrame = f - ((deficit * 75) >> 4); // Approximation avoiding floating-point division
                        if (deficitFrame < gasDeficitFrame)
                        {
                            gasDeficitFrame = deficitFrame;
                            gasDeficit = deficit;
                        }
                    }
                }
            }
            else
            // There are prerequisites that require gas
            {
                // Get the gas deficit if we produce the item and prerequisites on schedule
                int gasNeeded = prerequisiteCost + gasCost;
                auto prerequisiteIt = prerequisiteItems.rbegin();
                for (int f = PREDICT_FRAMES - 1; f >= 0; f--)
                {
                    int deficit = gasNeeded - gas[f];
                    if (deficit > 0)
                    {
                        int deficitFrame = f - ((deficit * 75) >> 4); // Approximation avoiding floating-point division
                        if (deficitFrame < gasDeficitFrame)
                        {
                            gasDeficitFrame = deficitFrame;
                            gasDeficit = deficit;
                        }
                    }

                    if (f == item.startFrame) gasNeeded -= gasCost;
                    while (prerequisiteIt != prerequisiteItems.rend() && f == (*prerequisiteIt)->startFrame)
                    {
                        gasNeeded -= (*prerequisiteIt)->gasPrice();
                        prerequisiteIt++;
                    }

                    if (gasNeeded == 0) break;
                }
            }

            // If we have enough gas now, just return
            if (gasDeficit == 0 || gasDeficitFrame == PREDICT_FRAMES) return true;

            // We are gas blocked, so there are three options:
            // - Shift one or more queued refineries earlier
            // - Add a refinery
            // - Shift the start frame of this item and the prerequisites to where we have gas
            // TODO: Also adjust number of gas workers
            auto refineryType = BWAPI::Broodwar->self()->getRace().getRefinery();

            // Compute how many frames of 3 workers collecting gas is needed to resolve the deficit
            int gasFramesNeeded = (int) ((double) gasDeficit / (GAS_PER_WORKER_FRAME * 3));

            // Look for refineries we can move earlier
            for (auto refineryIt = committedItems.begin(); gasFramesNeeded > 0 && refineryIt != committedItems.end();)
            {
                if (!(*refineryIt)->is(refineryType) || (*refineryIt)->queuedBuilding)
                {
                    refineryIt++;
                    continue;
                }

                // What frame do we want to move this refinery back to?
                // Will either be gasFramesNeeded frames before the current start frame (if it is already completing before the block),
                // or the build time of a refinery before the frame where we have the gas deficit
                int desiredStartFrame = std::max(0,
                                                 std::min((*refineryIt)->startFrame - gasFramesNeeded,
                                                          gasDeficitFrame - UnitUtil::BuildTime(refineryType)));

                // Find the actual frame we can move it to
                // We check two things:
                // - We have enough minerals to start the refinery at its new start frame
                // - We have enough minerals after the refinery completes and workers are reassigned
                int actualStartFrame = desiredStartFrame;
                for (int f = desiredStartFrame; f < (*refineryIt)->startFrame && f < PREDICT_FRAMES - UnitUtil::BuildTime(refineryType); f++)
                {
                    // Start frame
                    if (minerals[f] < refineryType.mineralPrice())
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }

                    // Completion frame
                    int completionFrame = f + UnitUtil::BuildTime(refineryType);
                    int mineralsNeeded = (int) (3.0 * (f - actualStartFrame) * MINERALS_PER_WORKER_FRAME);
                    if (completionFrame < (*refineryIt)->startFrame) mineralsNeeded += refineryType.mineralPrice();
                    if (minerals[completionFrame] < mineralsNeeded)
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }
                }

                // If we could move it back, make the relevant adjustment
                int delta = (*refineryIt)->startFrame - actualStartFrame;
                if (delta > 0)
                {
                    int completionFrame = actualStartFrame + UnitUtil::BuildTime(refineryType);

                    // Reduce the needed gas frames if the move helped
                    if ((*refineryIt)->completionFrame < (gasDeficitFrame + gasFramesNeeded))
                        gasFramesNeeded -= delta;
                    else if (completionFrame < (gasDeficitFrame + gasFramesNeeded))
                        gasFramesNeeded -= gasDeficitFrame + gasFramesNeeded - completionFrame;

                    if (commit)
                    {
                        shiftOne(committedItems, *refineryIt, -delta);

                        // Restart the iteration as the iterator may have been invalidated
                        refineryIt = committedItems.begin();
                        continue;
                    }
                }

                refineryIt++;
            }

            // If we've resolved the gas block, return
            if (gasFramesNeeded <= 0) return true;

            // Add a refinery if possible
            if (!availableGeysers.empty())
            {
                // Ideal timing makes enough gas available to build the current item
                int desiredStartFrame = std::max(0, gasDeficitFrame - UnitUtil::BuildTime(refineryType));

                // Find the actual frame we can start it at
                // We check two things:
                // - We have enough minerals to start the refinery
                // - We have enough minerals after the refinery completes and workers are reassigned
                int actualStartFrame = desiredStartFrame;
                for (int f = desiredStartFrame; f < PREDICT_FRAMES - UnitUtil::BuildTime(refineryType); f++)
                {
                    // Start frame
                    if (minerals[f] < refineryType.mineralPrice())
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }

                    // Completion frame
                    int completionFrame = f + UnitUtil::BuildTime(refineryType);
                    int mineralsNeeded = refineryType.mineralPrice() + (int) (3.0 * (f - actualStartFrame) * MINERALS_PER_WORKER_FRAME);
                    if (minerals[completionFrame] < mineralsNeeded)
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }
                }

                // Queue it if it was possible to build
                if (actualStartFrame < PREDICT_FRAMES)
                {
                    int completionFrame = actualStartFrame + UnitUtil::BuildTime(refineryType);

                    if (commit)
                    {
                        // Spend minerals
                        spendResource(minerals, refineryType.mineralPrice(), actualStartFrame);

                        // Update mineral and gas collection
                        updateResourceCollection(minerals, completionFrame, -3, MINERALS_PER_WORKER_FRAME);
                        updateResourceCollection(gas, completionFrame, 3, GAS_PER_WORKER_FRAME);

                        // Commit
                        auto geyser = *committedItems.emplace(std::make_shared<ProductionItem>(refineryType, actualStartFrame));
                        geyser->buildLocation = *availableGeysers.begin();
                        availableGeysers.erase(availableGeysers.begin());
                    }

                    // Reduce the needed gas frames as appropriate
                    if (completionFrame < (gasDeficitFrame + gasFramesNeeded))
                        gasFramesNeeded -= gasDeficitFrame + gasFramesNeeded - completionFrame;
                }
            }

            // If we've resolved the gas block, return
            if (gasFramesNeeded <= 0) return true;

            // Find the frame where we have enough gas
            int f = frameWhenResourcesMet(item, prerequisiteItems, false);
            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            int delta = f - item.startFrame;
            if (delta > 0)
            {
                item.startFrame += delta;
                item.completionFrame += delta;
                shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);
            }

            return true;
        }

        bool shiftForSupply(ProductionItem &item, ProductionItemSet &prerequisiteItems, bool commit)
        {
            if (item.supplyRequired() == 0) return true;

            // Gather the frames where we will create a supply block by producing this item
            std::vector<int> supplyBlockFrames;

            bool blocked = false;
            for (int f = item.startFrame; f < PREDICT_FRAMES; f++)
            {
                if (supply[f] < item.supplyRequired())
                {
                    // Break out if we are at max supply - this is a supply block we cannot fix
                    if (totalSupply[f] >= 400) return false;

                    if (!blocked)
                    {
                        supplyBlockFrames.push_back(f);
                        blocked = true;
                    }
                }
                else
                    blocked = false;
            }

            // If there are no blocks, return now
            if (supplyBlockFrames.empty()) return true;

            // Try to resolve each supply block
            for (int f : supplyBlockFrames)
            {
                // Look for an existing pylon active at this time
                // We are currently ignoring nexuses as they are not primarily built for supply, and we want our producer to queue pylons while a
                // nexus is building when it is appropriate
                // TODO This probably introduces some issues when a nexus is about to complete
                std::shared_ptr<ProductionItem> supplyProvider = nullptr;
                bool supplyProviderQueued = false;
                for (auto &potentialSupplyProvider : committedItems)
                {
                    // Ignore anything that isn't a pylon or completes before the block
                    if (!potentialSupplyProvider->is(BWAPI::UnitTypes::Protoss_Pylon)) continue;
                    if (potentialSupplyProvider->completionFrame <= f) continue;

                    // If a pylon is already being built, push the item until the completion frame
                    // Rationale is that we can't move it earlier and can't build a new one faster
                    if (potentialSupplyProvider->queuedBuilding)
                    {
                        if (potentialSupplyProvider->completionFrame >= PREDICT_FRAMES) return false;

                        int delta = potentialSupplyProvider->completionFrame - item.startFrame;
                        item.startFrame += delta;
                        item.completionFrame += delta;
                        shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);

                        supplyProviderQueued = true;
                        break;
                    }

                    supplyProvider = potentialSupplyProvider;
                    break;
                }
                if (supplyProviderQueued) continue;

                // If there is one, attempt to move it earlier
                if (supplyProvider)
                {
                    // Try to shift the supply provider so that it completes at the time of the block
                    int desiredStartFrame = std::max(0, f - buildTime(supplyProvider->type));

                    // Determine how far we can move the supply provider back and still have minerals
                    int mineralsNeeded = supplyProvider->mineralPrice() + item.mineralPrice();
                    int mineralFrame;
                    for (mineralFrame = supplyProvider->startFrame - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                    {
                        if (minerals[mineralFrame] < mineralsNeeded) break;
                        if (mineralFrame == item.startFrame) mineralsNeeded -= item.mineralPrice();
                    }
                    mineralFrame++;

                    int pylonDelta = mineralFrame - supplyProvider->startFrame;

                    // If the supply provider couldn't be moved back to resolve the block completely, shift the item
                    if (mineralFrame > desiredStartFrame)
                    {
                        int itemDelta = supplyProvider->completionFrame + pylonDelta - item.startFrame;
                        item.startFrame += itemDelta;
                        item.completionFrame += itemDelta;

                        // Break out if the start frame goes outside the window
                        if (item.startFrame >= PREDICT_FRAMES) return false;

                        shiftAll(prerequisiteItems, prerequisiteItems.begin(), itemDelta);
                    }

                    // If we could move the pylon, make the relevant adjustments
                    if (pylonDelta < 0 && commit)
                    {
                        shiftOne(committedItems, supplyProvider, pylonDelta);
                    }

                    // Continue to the next supply block
                    continue;
                }

                // There was no active supply provider to move
                // Let's see when we have the resources to queue a new one

                // Find the frame when we have enough minerals to produce the pylon
                int desiredStartFrame = std::max(0, f - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                int mineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() + item.mineralPrice();
                int mineralFrame;
                for (mineralFrame = PREDICT_FRAMES - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                {
                    if (minerals[mineralFrame] < mineralsNeeded) break;
                    if (mineralFrame == item.startFrame) mineralsNeeded -= item.mineralPrice();
                }
                mineralFrame++;

                // If the pylon can't complete within our prediction window, don't produce the item
                int completionFrame = mineralFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
                if (completionFrame >= PREDICT_FRAMES) return false;

                // Detect the case where the supply block blocks the current item and it is better to delay the previous item
                // to resolve the supply block earlier
                // TODO: Figure out if we need to re-implement this somehow
                /*
                if (f == item.startFrame && mineralFrame != desiredStartFrame && it != goalItems.begin())
                {
                    // Find the previous item
                    auto previousIt = goalItems.begin();
                    for (; previousIt != goalItems.end() && previousIt != it; previousIt++) {}

                    if (previousIt != goalItems.end() && (*previousIt)->startFrame < mineralFrame)
                    {
                        auto previous = **previousIt;

                        // First figure out when we can produce the pylon given we didn't produce the previous item
                        int pylonMineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() - previous.mineralPrice();
                        int pylonMineralFrame;
                        for (pylonMineralFrame = mineralFrame; pylonMineralFrame >= desiredStartFrame; pylonMineralFrame--)
                        {
                            if (minerals[pylonMineralFrame] < pylonMineralsNeeded) break;
                            if (pylonMineralFrame == previous.startFrame) mineralsNeeded += previous.mineralPrice();
                        }
                        pylonMineralFrame++;

                        // Now check if this would allow us to produce the current item earlier
                        if (std::max(mineralFrame - previous.startFrame, pylonMineralFrame - desiredStartFrame) < (mineralFrame - desiredStartFrame))
                        {
                            // Shift the items starting with the previous item
                            shiftAll(goalItems, previousIt, mineralFrame - previous.startFrame);
                            f = item.startFrame;

                            // Pylon is now being produced at the new frame we found
                            mineralFrame = pylonMineralFrame;

                            Log::Debug() << "Shifting pylon build frame ahead of previous item";
                        }
                    }
                }
                */

                // Queue the supply provider
                if (commit)
                {
                    committedItems.emplace(std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Pylon, mineralFrame));
                    addProvidedSupply(BWAPI::UnitTypes::Protoss_Pylon.supplyProvided(), completionFrame);
                    spendResource(minerals, BWAPI::UnitTypes::Protoss_Pylon.mineralPrice(), mineralFrame);
                }

                // If the block could not be resolved completely, shift the item to start when the supply provider completes
                if (mineralFrame > desiredStartFrame)
                {
                    int delta = completionFrame - item.startFrame;
                    item.startFrame += delta;
                    item.completionFrame += delta;
                    shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);
                }

                // Queueing a pylon always resolves all later supply blocks
                return true;
            }

            return true;
        }

        void commitItem(const std::shared_ptr<ProductionItem> &item)
        {
            // Spend the resources
            spendResource(minerals, item->mineralPrice(), item->startFrame);
            if (item->gasPrice() > 0) spendResource(gas, item->gasPrice(), item->startFrame);
            if (item->supplyRequired() > 0) spendResource(supply, item->supplyRequired(), item->startFrame);

            // If the item is a worker, assume it will go on minerals when complete
            if (item->is(BWAPI::Broodwar->self()->getRace().getWorker()))
                updateResourceCollection(minerals, item->completionFrame, 1, MINERALS_PER_WORKER_FRAME);

            // If the item is a refinery, make three workers available to it when it finishes
            if (item->is(BWAPI::Broodwar->self()->getRace().getRefinery()))
            {
                framesWithReassignableMineralWorker.insert(item->completionFrame);
                framesWithReassignableMineralWorker.insert(item->completionFrame);
                framesWithReassignableMineralWorker.insert(item->completionFrame);
            }

            // If the item provides supply, add it
            if (item->supplyProvided() > 0)
                addProvidedSupply(item->supplyProvided(), item->startFrame);

            // Commit
            committedItems.insert(item);
        }

        bool resolveResourceBlocks(const std::shared_ptr<ProductionItem> &item, ProductionItemSet &prerequisiteItems, bool commit)
        {
            if (!shiftForMinerals(*item, prerequisiteItems) ||
                !shiftForGas(*item, prerequisiteItems, commit) ||
                !shiftForSupply(*item, prerequisiteItems, commit))
            {
                return false;
            }

            // Guard against endless loops if there's a logic bug in one of the shift methods
            if (item->startFrame >= PREDICT_FRAMES) return false;

            // Shift if the producer is busy at the start frame
            // This can happen if the producer was able to insert this item earlier, but we didn't have resources for it at the time
            if (item->producer)
            {
                int delta = availableAt(item->type, item->startFrame, item->producer) - item->startFrame;
                if (delta > 0)
                {
                    item->startFrame += delta;
                    item->completionFrame += delta;
                    shiftAll(prerequisiteItems, prerequisiteItems.begin(), delta);

                    // Also set the availability of the producer later so it won't be considered for inserting the next item
                    item->producer->availableFrom = item->completionFrame;
                }
            }

            if (!commit) return true;

            // If there are prerequisites, commit them now
            for (const auto &prerequisiteItem : prerequisiteItems)
            {
                commitItem(prerequisiteItem);
            }

            // Commit this item
            commitItem(item);
            return true;
        }

        // Pulls refineries earlier if there are minerals available to do so
        void pullRefineries()
        {
            auto refineryType = BWAPI::Broodwar->self()->getRace().getRefinery();
            for (auto it = committedItems.begin(); it != committedItems.end(); it++)
            {
                auto &item = **it;

                if (!item.is(refineryType)) continue;
                if (item.queuedBuilding) continue;
                if (item.completionFrame >= PREDICT_FRAMES) continue;

                // Look up the lowest number of minerals we have after the refinery is completed
                int lowestMinerals = INT_MAX;
                for (int f = item.completionFrame; f < PREDICT_FRAMES; f++)
                {
                    lowestMinerals = std::min(lowestMinerals, minerals[f]);
                }

                if (lowestMinerals <= 0) continue;

                // Use this to deduce how many frames of 3 workers mining we can afford to lose, with a safety buffer
                int availableFrames = (int) ((double) lowestMinerals / (3.0 * MINERALS_PER_WORKER_FRAME * 0.9));

                // Find a frame earlier than the current start frame within this constraint where we have enough minerals
                int startFrame = item.startFrame;
                for (int f = item.startFrame - 1; f >= std::max(0, item.startFrame - availableFrames); f--)
                {
                    if (minerals[f] < refineryType.mineralPrice()) break;
                    startFrame = f;
                }

                // Update the item if needed
                if (startFrame < item.startFrame)
                {
                    shiftOne(committedItems, *it, startFrame - item.startFrame);

                    // Restart the iteration as we may have changed the ordering
                    pullRefineries();
                    return;
                }
            }
        }

        // Pulls supply providers earlier if there are minerals available to do so
        void pullSupplyProviders()
        {
            for (const auto &item : committedItems)
            {
                if (!item->is(BWAPI::UnitTypes::Protoss_Pylon) && !item->is(BWAPI::UnitTypes::Protoss_Nexus)) continue;
                if (item->queuedBuilding) continue;
                if (item->completionFrame >= PREDICT_FRAMES) continue;

                int mineralCost = item->mineralPrice();

                // Pull nexuses as far as possible, pylons up to 48 frames
                int limit = 0;
                if (item->is(BWAPI::UnitTypes::Protoss_Pylon)) limit = std::max(0, item->startFrame - 48);

                int f = item->startFrame - 1;
                for (; f >= limit; f--)
                {
                    if (minerals[f] < mineralCost) break;
                }
                item->startFrame = f + 1;

                // This runs after everything has been scheduled, so we don't need to bother with updating minerals, completion frame, etc.
            }
        }

        void handleGoal(Type type,
                        const ProductionLocation &location,
                        int countToProduce,
                        int producerLimit,
                        BWAPI::UnitType prerequisite,
                        MyUnit reservedBuilder,
                        int frame)
        {
            int toProduce = countToProduce;

            // For some logic we need to know which variant type we are producing, so reference them here
            auto unitType = std::get_if<BWAPI::UnitType>(&type);
            auto upgradeOrTechType = std::get_if<UpgradeOrTechType>(&type);
            auto buildLocation = std::get_if<BuildingPlacement::BuildLocation>(&location);

            // Do some preprocessing of buildings to remove them if a matching building already exists in the committed items
            // This happens if we are requesting a building that has been queued by the builder
            if (unitType && unitType->isBuilding())
            {
                // Verify we have a build location, which is required for buildings
                if (!buildLocation)
                {
                    Log::Get() << "ERROR: Trying to produce building without specifying a location";
                    return;
                }

                // Scan for a matching building in the committed item set
                for (auto &otherItem : committedItems)
                {
                    // Compare unit types
                    auto committedItemUnitType = std::get_if<BWAPI::UnitType>(&otherItem->type);
                    if (!committedItemUnitType || *committedItemUnitType != *unitType) continue;

                    // If the tile positions are the same, skip this item
                    if (buildLocation->location.tile == otherItem->buildLocation.location.tile) return;

                    // If the other item is a pylon that does not already have a location, use this one instead
                    // This case happens if we've queued a pylon for supply earlier and now get a request for a specific
                    // pylon location - we may as well just build one
                    // One exception is if we don't have any pylons currently - we don't want to steal our start block pylon
                    if (*unitType == BWAPI::UnitTypes::Protoss_Pylon && !otherItem->buildLocation.location.tile.isValid()
                        && Units::countAll(BWAPI::UnitTypes::Protoss_Pylon) > 0)
                    {
                        otherItem->estimatedWorkerMovementTime = buildLocation->builderFrames;
                        otherItem->buildLocation = *buildLocation;
                        otherItem->reservedBuilder = std::move(reservedBuilder);
                        return;
                    }
                }
            }

            BWAPI::UnitType producerType;
            if (unitType)
                producerType = unitType->whatBuilds().first;
            else if (upgradeOrTechType)
                producerType = upgradeOrTechType->whatUpgradesOrResearches();

            ProductionItemSet prerequisiteItems;

            // Step 1: Ensure we have all buildings we need to build this type

            // Add missing prerequisites for the type
            if (unitType) addMissingPrerequisites(prerequisiteItems, *unitType, location, producerType);
            if (prerequisite != BWAPI::UnitTypes::None)
            {
                addBuildingIfIncomplete(prerequisiteItems,
                                        prerequisite,
                                        std::monostate(),
                                        producerType,
                                        0,
                                        true);
            }

            // Unless this item is a building, ensure we have something that can produce it
            if (!unitType || !unitType->isBuilding()) addBuildingIfIncomplete(prerequisiteItems, producerType, location, producerType);

            // Shift the buildings so the frames start at 0
            normalizeStartFrame(prerequisiteItems);

            // Resolve duplicates, keeping the earliest one of each type
            // Also record when the last prerequisite will finish
            int prerequisitesAvailable = resolveDuplicates(prerequisiteItems);

            // Reserve positions for all buildings
            reserveBuildPositions(prerequisiteItems);

            // If the last goal item has been pushed back, update the prerequisitesAvailable frame
            if (!prerequisiteItems.empty()) prerequisitesAvailable = std::max(prerequisitesAvailable, (*prerequisiteItems.rbegin())->completionFrame);

            // Push the prerequisitesAvailable frame if we want to build the item later
            prerequisitesAvailable = std::max(prerequisitesAvailable, frame - currentFrame);

            // Buildings can now be handled immediately - we only ever produce one at a time and require a specific location
            if (unitType && unitType->isBuilding())
            {
                auto buildingItem = std::make_shared<ProductionItem>(type, prerequisitesAvailable, location);
                buildingItem->estimatedWorkerMovementTime = buildLocation->builderFrames;
                buildingItem->buildLocation = *buildLocation;
                buildingItem->reservedBuilder = std::move(reservedBuilder);
                resolveResourceBlocks(buildingItem, prerequisiteItems, true);
                return;
            }

            // Step 2: Collect producers
            std::vector<std::shared_ptr<Producer>> producers;

            // Planned producers
            for (auto &item : prerequisiteItems)
            {
                if (item->is(producerType) && canProduceFrom(type, location, *item))
                    producers.emplace_back(std::make_shared<Producer>(item, std::max(prerequisitesAvailable, item->completionFrame)));
            }

            // Committed producers
            for (auto &item : committedItems)
            {
                if (!item->is(producerType) || !canProduceFrom(type, location, *item)) continue;

                if (item->queuedBuilding && item->queuedBuilding->unit)
                {
                    auto existingProducerIt = existingProducers.find(item->queuedBuilding->unit);
                    if (existingProducerIt != existingProducers.end())
                    {
                        existingProducerIt->second->availableFrom = std::max(prerequisitesAvailable, item->completionFrame);
                        producers.push_back(existingProducerIt->second);
                        continue;
                    }

                    auto producer = std::make_shared<Producer>(item, std::max(prerequisitesAvailable, item->completionFrame));
                    producers.push_back(producer);
                    existingProducers[item->queuedBuilding->unit] = producer;
                }
                else
                {
                    producers.emplace_back(std::make_shared<Producer>(item, std::max(prerequisitesAvailable, item->completionFrame)));
                }
            }

            // Completed producers
            for (const auto &unit : Units::allMine())
            {
                if (!unit->exists() || !unit->completed) continue;
                if (unit->type != producerType) continue;
                if (!canProduceFrom(type, location, unit)) continue;

                // If we have sent the train command to a producer, but the unit isn't created yet, then reduce the number we need to build
                // This is not perfect, as it will reduce the count of all production goals targeting this unit type, but it's probably
                // rare that we have multiple goals with limited numbers of the same unit type anyway.
                if (unitType && toProduce != -1 && unit->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Train &&
                    (currentFrame - unit->lastCommandFrame - 1) < BWAPI::Broodwar->getLatencyFrames() &&
                    unit->bwapiUnit->getLastCommand().getUnitType() == *unitType)
                {
                    toProduce--;
                    if (toProduce == 0) return;
                }

                int remainingTrainTime = -1;
                if (unitType)
                {
                    remainingTrainTime = getRemainingBuildTime(unit);
                }
                else if (upgradeOrTechType)
                {
                    remainingTrainTime = upgradeOrTechType->isTechType()
                                         ? getRemainingResearchTime(unit)
                                         : getRemainingUpgradeTime(unit);
                }

                // If this producer is already created when handling a previous production goal, reference the existing object
                auto existingProducerIt = existingProducers.find(unit);
                if (existingProducerIt != existingProducers.end())
                {
                    existingProducerIt->second->availableFrom = std::max(prerequisitesAvailable, remainingTrainTime + 1);
                    producers.push_back(existingProducerIt->second);
                    continue;
                }

                auto producer = std::make_shared<Producer>(unit, std::max(prerequisitesAvailable, remainingTrainTime + 1));
                producers.push_back(producer);
                existingProducers[unit] = producer;
            }

            // If we at this point have no producers, it means we have buildings that can produce the item, but not in the location we need
            // So add a new producer at the correct location to the prerequisite items
            if (producers.empty())
            {
                int startFrame = std::max(0, prerequisitesAvailable - UnitUtil::BuildTime(producerType));
                auto producerItem = std::make_shared<ProductionItem>(producerType, startFrame, location);
                prerequisiteItems.insert(producerItem);

                reserveBuildPositions(prerequisiteItems);

                producers.emplace_back(std::make_shared<Producer>(producerItem, std::max(prerequisitesAvailable, producerItem->completionFrame)));
            }

            // Step 3: Repeatedly commit a unit from the earliest producer available, or a new one if applicable, until we have built enough
            //         or don't have resources for more

            bool committedSomething = false;
            while (toProduce == -1 || toProduce > 0)
            {
                // Find the earliest available producer
                std::shared_ptr<Producer> bestProducer = nullptr;
                int bestFrame = PREDICT_FRAMES;
                for (auto &producer : producers)
                {
                    int available = availableAt(type, prerequisitesAvailable, producer);
                    if (available < bestFrame)
                    {
                        bestFrame = available;
                        bestProducer = producer;
                    }
                }

                // Attempt to create an item from the best producer
                std::shared_ptr<ProductionItem> bestProducerItem = nullptr;
                if (bestProducer)
                {
                    // In some situations we want to commit immediately:
                    // - There are prerequisite items to produce
                    //   - Usually means this is the first item of the type, and a producer is in the prerequisite items set
                    // - We are producing an unlimited number of units
                    //   - We want to saturate production from existing producers before adding new ones
                    // - We are already at our producer limit
                    bool commitImmediately = !prerequisiteItems.empty() || producers.size() >= producerLimit || toProduce == -1;

                    // Create the item and resolve resource blocks
                    bestProducerItem = std::make_shared<ProductionItem>(type, bestFrame, location, bestProducer);
                    bool result = resolveResourceBlocks(bestProducerItem, prerequisiteItems, commitImmediately);

                    // If the item was created and committed, continue the loop now
                    if (commitImmediately && result)
                    {
                        committedSomething = true;

                        if (toProduce > 0) toProduce--;
                        bestProducer->items.insert(bestProducerItem);

                        // If we have prerequisites, we may need to adjust the frame when prerequisites are available, as
                        // they may have shifted
                        if (!prerequisiteItems.empty())
                        {
                            for (auto &prerequisiteItem : prerequisiteItems)
                            {
                                prerequisitesAvailable = std::max(prerequisitesAvailable, prerequisiteItem->completionFrame);
                            }

                            // Clear the prerequisites, as they have now been committed
                            prerequisiteItems.clear();
                        }

                        continue;
                    }

                    // If we wanted to commit the item but couldn't, continue only if we are producing an unlimited number of units
                    // In the other two cases we can't produce the unit
                    if (commitImmediately && (!prerequisiteItems.empty() || producers.size() >= producerLimit)) break;

                    // If the item could not be created, clear it
                    if (!result) bestProducerItem = nullptr;
                }
                else if (producers.size() >= producerLimit) break;

                // Create a new producer
                ProductionItemSet newProducerPrerequisites;
                auto producerItem = *newProducerPrerequisites.emplace(std::make_shared<ProductionItem>(producerType,
                                                                                                       std::max(0,
                                                                                                                prerequisitesAvailable
                                                                                                                - UnitUtil::BuildTime(producerType)),
                                                                                                       location));
                auto newProducer = std::make_shared<Producer>(producerItem, producerItem->completionFrame);

                // Create an item from the producer
                auto newProducerItem = std::make_shared<ProductionItem>(type, producerItem->completionFrame, location, newProducer);
                bool result = resolveResourceBlocks(newProducerItem, newProducerPrerequisites, false);
                if (!result) newProducerItem = nullptr;

                // If we couldn't produce an item, break now
                if (!bestProducerItem && !newProducerItem) break;

                // Commit the one that could be produced earliest
                if (bestProducerItem && (!newProducerItem || bestProducerItem->startFrame <= newProducerItem->startFrame))
                {
                    if (!resolveResourceBlocks(bestProducerItem, prerequisiteItems, true)) break;
                    bestProducer->items.insert(bestProducerItem);
                }
                else
                {
                    reserveBuildPositions(newProducerPrerequisites);
                    if (!resolveResourceBlocks(newProducerItem, newProducerPrerequisites, true)) break;
                    producers.push_back(newProducer);
                    newProducer->items.insert(newProducerItem);
                }

                committedSomething = true;

                if (toProduce > 0) toProduce--;
            }

            // If we have prerequisites but didn't produce anything, try to produce the first prerequisite instead
            // This situation comes up when we don't predict to be able to produce the unit within our prediction window
            // So producing the first prerequisite allows us to keep things moving until the item can be produced at a later frame
            if (!prerequisiteItems.empty() && !committedSomething)
            {
                ProductionItemSet emptyPrerequisites;
                resolveResourceBlocks(*prerequisiteItems.begin(), emptyPrerequisites, true);
            }
        }

#ifndef DEBUG
    }
#endif

    void update()
    {
        // The overall objective is, for each production goal, to determine what should be produced next and
        // do so whenever this does not delay a higher-priority goal.

        committedItems.clear();

        initializeResources();

#if DEBUG_WRITE_SUBGOALS
        int count = 0;
        write(committedItems, (std::ostringstream() << "producergoal" << count).str());
#endif

        // While running through the goals, keep track of whether we have an unlimited production goal that needs gas
        // In emergency situations we might cancel all production of units that need gas, in which case we want to transfer workers off of gas
        // Transferring workers onto gas is handled in the producer logic
        bool hasUnlimitedGasGoal = false;
        int availableGas = BWAPI::Broodwar->self()->gas();
        for (auto goal : Strategist::currentProductionGoals())
        {
            if (auto unitProductionGoal = std::get_if<UnitProductionGoal>(&goal))
            {
                if (unitProductionGoal->countToProduce() == -1 && unitProductionGoal->unitType().gasPrice() > 0)
                {
                    hasUnlimitedGasGoal = true;
                    availableGas = -1;
                }

                handleGoal(unitProductionGoal->unitType(),
                           unitProductionGoal->getLocation(),
                           unitProductionGoal->countToProduce(),
                           unitProductionGoal->getProducerLimit(),
                           BWAPI::UnitTypes::None,
                           unitProductionGoal->getReservedBuilder(),
                           unitProductionGoal->getFrame());
            }
            else if (auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&goal))
            {
                handleGoal(upgradeProductionGoal->upgradeType(),
                           std::monostate(),
                           1,
                           upgradeProductionGoal->getProducerLimit(),
                           upgradeProductionGoal->prerequisiteForNextLevel(),
                           nullptr,
                           upgradeProductionGoal->getFrame());
            }
            else
            {
                Log::Get() << "ERROR: Unknown variant type for ProductionGoal";
            }

#if DEBUG_WRITE_SUBGOALS
            count++;
            write(committedItems, (std::ostringstream() << "producergoal" << count).str());
#endif
        }

        if (!hasUnlimitedGasGoal)
        {
            for (const auto &item : committedItems)
            {
                availableGas -= item->gasPrice();
            }
        }

        // Set gas collection appropriately
        if (reassignedMineralWorkersAtStartFrame > 0)
        {
            Workers::setDesiredGasWorkerDelta(reassignedMineralWorkersAtStartFrame);
        }
        else if (availableGas >= 0)
        {
            Workers::setDesiredGasWorkerDelta(-Workers::reassignableGasWorkers());
        }

        if (hasUnlimitedGasGoal)
        {
            pullRefineries();
        }

        // Pylons are often built a bit too late, since we don't accurately simulate mineral collection and the build worker
        // can be delayed. So pull pylons a bit earlier whenever we have the resources for it.
        // Also pulls nexuses since we always want them build as early as possible.
        pullSupplyProviders();

        // When we are maxed, ensure we have enough gateways to quickly replace losses
        if (totalSupply[0] >= 400 && supply[0] <= 4)
        {
            // Count the gateways we already have
            int gateways = Units::countCompleted(BWAPI::UnitTypes::Protoss_Gateway) +
                           Builder::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Gateway).size();

            // Build up to 25 gateways, but keep 2 build locations reserved for other types of buildings
            // We might have a need to build air units or tech later on in the game
            unsigned long desiredGateways =
                    std::min(25 - gateways,
                             (int)buildLocations[BuildingPlacement::Neighbourhood::AllMyBases][BWAPI::UnitTypes::Protoss_Gateway.tileWidth()].size()
                             - 2);

            // Scale up to 25 gateways as our bank allows
            for (int gateway = gateways + 1; gateway <= gateways + desiredGateways; gateway++)
            {
                int requiredMinerals = gateway * BWAPI::UnitTypes::Protoss_Dragoon.mineralPrice() + BWAPI::UnitTypes::Protoss_Gateway.mineralPrice();
                int requiredGas = gateway * BWAPI::UnitTypes::Protoss_Dragoon.gasPrice() + BWAPI::UnitTypes::Protoss_Gateway.gasPrice();

                // Find the frame where it makes sense to build this gateway
                int startFrame;
                for (startFrame = PREDICT_FRAMES - 1; startFrame >= 0; startFrame--)
                {
                    if (minerals[startFrame] < requiredMinerals) break;
                    if (minerals[startFrame] < requiredGas) break;
                }
                startFrame++;
                if (startFrame >= PREDICT_FRAMES) break;

                // Commit the item, if we have a build location
                auto item = std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Gateway, startFrame, BuildingPlacement::Neighbourhood::AllMyBases);
                auto itemInSet = ProductionItemSet{item};
                reserveBuildPositions(itemInSet);
                if (!item->buildLocation.location.tile.isValid()) break;
                committedItems.insert(item);
            }
        }

#if OUTPUT_BUILD_QUEUE
        write(committedItems, "producer");
#endif

        // Issue build commands
        bool firstUnbuiltItem = true;
        for (auto &item : committedItems)
        {
            // Units
            if (auto unitType = std::get_if<BWAPI::UnitType>(&item->type))
            {
                // Produce units desired at frame 0
                if (item->producer && item->producer->existing && item->startFrame <= BWAPI::Broodwar->getRemainingLatencyFrames())
                {
                    auto result = item->producer->existing->train(*unitType);
                    if (result)
                        Log::Debug() << "Sent command to train " << (*unitType) << " from " << item->producer->existing->type << " @ "
                                     << item->producer->existing->getTilePosition();
                }

                // Pylons may not already have a build location
                if (item->is(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    choosePylonBuildLocation(*item);
                }

                // Queue buildings when we expect the builder will arrive at the desired start frame or later
                if (unitType->isBuilding() && !item->queuedBuilding &&
                    (item->startFrame - item->buildLocation.builderFrames) <= BWAPI::Broodwar->getRemainingLatencyFrames())
                {
                    // TODO: Should be resolved earlier
                    if (!item->buildLocation.location.tile.isValid())
                    {
                        Log::Get() << "ERROR: No build location for " << *unitType;
                        continue;
                    }

                    // Special case: never try to build a prerequisite cybernetics core before the assimilator has been queued
                    // This may happen because a worker needs to travel further to the core build location
                    // It really confuses our resource scheduling and would usually result in pushing back the assimilator
                    if (item->isPrerequisite && item->is(BWAPI::UnitTypes::Protoss_Cybernetics_Core)
                        && Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0
                        && Builder::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Assimilator).empty())
                    {
                        continue;
                    }

                    int arrivalFrame = 0;
                    auto builder = item->reservedBuilder
                                   ? item->reservedBuilder
                                   : Builder::getBuilderUnit(item->buildLocation.location.tile, *unitType, &arrivalFrame);
                    if (builder && arrivalFrame >= item->startFrame)
                    {
                        Builder::build(*unitType, item->buildLocation.location.tile, builder, currentFrame + item->startFrame);
                    }
                }

                // If the first unbuilt item in the queue is a building, clear the desired start frame if the worker is ready to build it
                // At this point we have made sure we had time to produce more important things while the worker was in transit,
                // so there's no reason for it to wait until the original scheduled time
                if (firstUnbuiltItem && item->queuedBuilding && item->queuedBuilding->desiredStartFrame > 0 && item->queuedBuilding->builderReady())
                {
                    item->queuedBuilding->desiredStartFrame = 0;
                }

                // Update the flag
                if (!item->queuedBuilding || !item->queuedBuilding->isConstructionStarted())
                {
                    firstUnbuiltItem = false;
                }
            }

                // Upgrades
            else if (auto upgradeOrTechType = std::get_if<UpgradeOrTechType>(&item->type))
            {
                // Upgrade if start frame is now
                if (item->producer && item->producer->existing && item->startFrame <= BWAPI::Broodwar->getRemainingLatencyFrames())
                {
                    if (upgradeOrTechType->isTechType())
                    {
                        if (item->producer->existing->research(upgradeOrTechType->techType))
                        {
                            Log::Get() << "Started research: " << upgradeOrTechType->techType;
                        }
                    }
                    else
                    {
                        if (item->producer->existing->upgrade(upgradeOrTechType->upgradeType))
                        {
                            Log::Get() << "Started upgrade: " << upgradeOrTechType->upgradeType
                                       << " (" << (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeOrTechType->upgradeType) + 1) << ")";
                        }
                    }
                }
            }
        }
    }
}
