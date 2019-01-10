#include "Producer.h"

#include <array>

#include "Strategist.h"
#include "Units.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Workers.h"
#include "UnitUtil.h"

/*
Overall TODOs:
- LF
- Support building a specific number of units as quickly as possible
  - Update workers so they can be queued if they recoup their cost before this becomes a problem
- Support maximizing production or pressure on enemy at a certain frame
- Support building outside of main base
*/
namespace Producer
{
#ifndef _DEBUG
    namespace
    {
#endif
        // How many frames in the future to consider when predicting future production needs
        const int PREDICT_FRAMES = 8000;

        const double MINERALS_PER_WORKER_FRAME = 0.0465;
        const double GAS_PER_WORKER_FRAME = 0.071;

        std::array<int, PREDICT_FRAMES> minerals;
        std::array<int, PREDICT_FRAMES> gas;
        std::array<int, PREDICT_FRAMES> supply;
        std::map<BuildingPlacement::Neighbourhood, std::map<int, BuildingPlacement::BuildLocationSet>> buildLocations;
        BuildingPlacement::BuildLocationSet availableGeysers;

        const BuildingPlacement::BuildLocation InvalidBuildLocation(BWAPI::TilePositions::Invalid, 0, 0);

        using Type = std::variant<BWAPI::UnitType, BWAPI::UpgradeType>;

        int buildTime(Type type)
        {
            if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                return UnitUtil::BuildTime(*unitType);
            if (auto upgradeType = std::get_if<BWAPI::UpgradeType>(&type))
                return upgradeType->upgradeTime(BWAPI::Broodwar->self()->getUpgradeLevel(*upgradeType) + 1);
            return 0;
        }

        struct Producer;
        struct ProductionItem
        {
            struct cmp
            {
                bool operator()(const std::shared_ptr<ProductionItem> & a, const std::shared_ptr<ProductionItem> & b) const
                {
                    return a->startFrame < b->startFrame 
                        || (!(b->startFrame < a->startFrame) && a->completionFrame < b->completionFrame)
                        || (!(b->startFrame < a->startFrame) && !(b->completionFrame < a->completionFrame) && a < b);
                }
            };

            // Type, can be a unit or upgrade
            Type type;

            int mineralPrice()
            {
                // Buildings require a worker to be taken off minerals for a while, so we estimate the impact
                // In some cases this will be too pessimistic, as we can re-use the same worker for multiple jobs
                return (int)(2.0 * estimatedWorkerMovementTime * MINERALS_PER_WORKER_FRAME) + std::visit([](auto&& arg) {return arg.mineralPrice(); }, type);
            }

            int gasPrice()
            {
                return std::visit([](auto&& arg) {return arg.gasPrice(); }, type);
            }

            int supplyProvided()
            {
                if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                    return unitType->supplyProvided();
                return 0;
            }

            int supplyRequired()
            {
                if (auto unitType = std::get_if<BWAPI::UnitType>(&type))
                    return unitType->supplyRequired();
                return 0;
            }

            bool is(BWAPI::UnitType unitType)
            {
                auto _unitType = std::get_if<BWAPI::UnitType>(&type);
                return _unitType && *_unitType == unitType;
            }

            mutable int                              startFrame;
            mutable int                              completionFrame;

            // Fields applicable to non-buildings
            const std::shared_ptr<Producer>          producer;

            // Fields applicable to buildings
            bool                                     isPrerequisite;
            Building *                               queuedBuilding;
            mutable BuildingPlacement::BuildLocation buildLocation;
            mutable int                              estimatedWorkerMovementTime;

            ProductionItem(Type type, int startFrame, const std::shared_ptr<Producer> producer = nullptr, bool isPrerequisite = false)
                : type(type)
                , startFrame(startFrame)
                , completionFrame(startFrame + buildTime(type))
                , producer(producer)
                , isPrerequisite(isPrerequisite)
                , queuedBuilding(nullptr)
                , buildLocation(InvalidBuildLocation)
                , estimatedWorkerMovementTime(0)
            {}

            // Constructor for pending buildings
            ProductionItem(Building* queuedBuilding)
                : type(queuedBuilding->type)
                , startFrame(queuedBuilding->expectedFramesUntilStarted())
                , completionFrame(queuedBuilding->expectedFramesUntilCompletion())
                , producer(nullptr)
                , isPrerequisite(false)
                , queuedBuilding(queuedBuilding)
                , buildLocation(InvalidBuildLocation)
                , estimatedWorkerMovementTime(0)
            {}
        };
        typedef std::multiset<std::shared_ptr<ProductionItem>, ProductionItem::cmp> ProductionItemSet;

        struct Producer
        {
            std::shared_ptr<ProductionItem> queued;
            BWAPI::Unit                     existing;
            ProductionItemSet               items;

            Producer(const std::shared_ptr<ProductionItem> queued)
                : queued(queued)
                , existing(nullptr)
            {}

            Producer(BWAPI::Unit existing)
                : queued(nullptr)
                , existing(existing)
            {}
        };

        std::map<std::shared_ptr<ProductionItem>, std::shared_ptr<Producer>> queuedProducers;
        std::map<BWAPI::Unit, std::shared_ptr<Producer>> existingProducers;

        void write(ProductionItemSet & items)
        {
            auto itemLabel = [](const std::shared_ptr<ProductionItem> & item)
            {
                std::ostringstream label;
                std::visit([&label](auto&& arg) { label << arg; }, item->type);
                if (item->queuedBuilding && item->queuedBuilding->isConstructionStarted()) label << "^";
                if (item->queuedBuilding && !item->queuedBuilding->isConstructionStarted()) label << "*";
                return label.str();
            };

            // Output first row with the queue
            {
                Log::LogWrapper csv = Log::Csv("producer");

                int seconds = BWAPI::Broodwar->getFrameCount() / 24;
                csv << BWAPI::Broodwar->getFrameCount();
                csv << (seconds / 60);
                csv << (seconds % 60);

                for (auto & item : items)
                {
                    if (item->startFrame >= PREDICT_FRAMES) continue;
                    csv << itemLabel(item) << item->startFrame << item->completionFrame;
                    if (BWAPI::Broodwar->getFrameCount() < 2) Log::Debug() << itemLabel(item) << ": " << item->startFrame << ";" << item->completionFrame;
                }
            }

            // Output second row with resource counts
            {
                Log::LogWrapper csv = Log::Csv("producer");

                csv << BWAPI::Broodwar->self()->minerals();
                csv << BWAPI::Broodwar->self()->gas();
                csv << (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());

                for (auto & item : items)
                {
                    if (item->startFrame >= PREDICT_FRAMES) continue;
                    csv << minerals[item->startFrame] << gas[item->startFrame] << supply[item->startFrame];
                }
            }
        }

        void updateResourceCollection(std::array<int, PREDICT_FRAMES> & resource, int fromFrame, int workerChange, double baseRate)
        {
            double delta = (double)workerChange * baseRate;
            for (int f = fromFrame + 1; f < PREDICT_FRAMES; f++)
            {
                resource[f] += (int)(delta * (f - fromFrame));
            }
        }

        void spendResource(std::array<int, PREDICT_FRAMES> & resource, int fromFrame, int amount)
        {
            for (int f = fromFrame; f < PREDICT_FRAMES; f++)
            {
                resource[f] -= amount;
            }
        }

        void initializeResources(ProductionItemSet & committedItems)
        {
            // Fill mineral and gas with current rates
            int currentMinerals = BWAPI::Broodwar->self()->minerals();
            int currentGas = BWAPI::Broodwar->self()->gas();
            double mineralRate = (double)Workers::mineralWorkers() * MINERALS_PER_WORKER_FRAME;
            double gasRate = (double)Workers::gasWorkers() * GAS_PER_WORKER_FRAME;
            for (int f = 0; f < PREDICT_FRAMES; f++)
            {
                minerals[f] = currentMinerals + (int)(mineralRate * f);
                gas[f] = currentGas + (int)(gasRate * f);
            }

            // Fill supply with current supply count
            supply.fill(BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());

            // Assume workers being built will go to minerals
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->isCompleted()) continue;
                if (!unit->getType().isWorker()) continue;

                updateResourceCollection(minerals, unit->getRemainingBuildTime(), 1, MINERALS_PER_WORKER_FRAME);
            }

            // Adjust for pending buildings
            // This also adds all pending buildings to the committed item set
            // TODO: Should be able to change our minds and cancel pending buildings that are no longer needed
            for (auto pendingBuilding : Builder::allPendingBuildings())
            {
                // "Commit" the item and spend resources if the building isn't started
                auto item = *committedItems.emplace(std::make_shared<ProductionItem>(pendingBuilding));
                if (!pendingBuilding->isConstructionStarted())
                {
                    spendResource(minerals, item->startFrame, pendingBuilding->type.mineralPrice());
                    spendResource(gas, item->startFrame, pendingBuilding->type.gasPrice());
                }

                // Supply providers
                if (pendingBuilding->type.supplyProvided() > 0)
                {
                    spendResource(supply, item->completionFrame, -pendingBuilding->type.supplyProvided());
                }

                // Refineries are assumed to move three workers to gas on completion
                // TODO: Do this more dynamically
                if (pendingBuilding->type == BWAPI::Broodwar->self()->getRace().getRefinery())
                {
                    updateResourceCollection(minerals, item->completionFrame, -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, item->completionFrame, 3, GAS_PER_WORKER_FRAME);
                }
            }

            buildLocations = BuildingPlacement::getBuildLocations();
            availableGeysers = BuildingPlacement::availableGeysers();
            queuedProducers.clear();
            existingProducers.clear();
        }

        // Need a forward reference since the next two methods do mutual recursion
        void addMissingPrerequisites(ProductionItemSet & items, BWAPI::UnitType unitType, int startFrame = 0);

        void addBuildingIfIncomplete(ProductionItemSet & items, BWAPI::UnitType unitType, int baseFrame = 0, bool isPrerequisite = false)
        {
            if (Units::countCompleted(unitType) > 0) return;

            int startFrame = baseFrame - UnitUtil::BuildTime(unitType);
            items.emplace(std::make_shared<ProductionItem>(unitType, startFrame, nullptr, isPrerequisite));
            addMissingPrerequisites(items, unitType, startFrame);
        }

        // For the specified unit type, recursively checks what prerequisites are missing and adds them to the set
        // The frames are relative to the time when the first unit of the desired type can be produced
        void addMissingPrerequisites(ProductionItemSet & items, BWAPI::UnitType unitType, int startFrame)
        {
            for (auto typeAndCount : unitType.requiredUnits())
            {
                // Don't include workers
                if (typeAndCount.first.isWorker()) continue;

                addBuildingIfIncomplete(items, typeAndCount.first, startFrame, true);
            }
        }

        // Shifts one item in the given set by the given amount
        void shiftOne(ProductionItemSet & items, const std::shared_ptr<ProductionItem> item, int delta)
        {
            if (delta == 0) return;

            // Adjust resources

            if (item->supplyProvided() > 0)
            {
                spendResource(supply, item->completionFrame, item->supplyProvided());
                spendResource(supply, item->completionFrame + delta, -item->supplyProvided());
            }

            if (item->gasPrice() > 0)
            {
                spendResource(gas, item->startFrame, -item->gasPrice());
                spendResource(gas, item->startFrame + delta, item->gasPrice());
            }

            spendResource(minerals, item->startFrame, -item->mineralPrice());
            spendResource(minerals, item->startFrame + delta, item->mineralPrice());

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
        void shiftAll(ProductionItemSet & items, ProductionItemSet::iterator it, int delta)
        {
            if (delta < 1 || it == items.end()) return;

            int startFrame = (*it)->startFrame;

            for (; it != items.end(); it++)
            {
                auto & item = **it;

                item.startFrame += delta;
                item.completionFrame += delta;

                // If the producer was queued as part of this goal, and this item is the first item it is producing, then shift it too
                if (item.producer && item.producer->queued && !item.producer->queued->isPrerequisite && items.find(item.producer->queued) != items.end()
                    && !item.producer->items.empty() && *item.producer->items.begin() == *it)
                {
                    shiftOne(items, item.producer->queued, delta);
                }
            }
        }

        // Remove duplicate items from this goal's set and resolve conflicts with already-committed items
        void resolveDuplicates(ProductionItemSet & items, ProductionItemSet & committedItems)
        {
            std::set<Type> seen;
            for (auto it = items.begin(); it != items.end(); )
            {
                auto & item = **it;

                // There's already an item of this type in this goal's queue
                if (seen.find(item.type) != seen.end())
                {
                    it = items.erase(it);
                    continue;
                }

                // Check if there is a matching item in the committed item set
                bool handled = false;
                for (auto & committedItem : committedItems)
                {
                    if (committedItem->type != item.type) continue;

                    // We found a match

                    // Compute the difference in completion times
                    int delta = committedItem->completionFrame - item.completionFrame;

                    // Remove the item
                    it = items.erase(it);

                    // If the committed item completes later, shift all the later items in this goal's queue
                    // The rationale for this is that the committed item set is already optimized to produce everything
                    // as early as possible, so we know we can't move it earlier
                    shiftAll(items, it, delta);

                    handled = true;
                    break;
                }

                // We didn't find a match, so just register this type and continue
                if (!handled)
                {
                    seen.insert(item.type);
                    it++;
                }
            }
        }

        void choosePylonBuildLocation(BuildingPlacement::Neighbourhood neighbourhood, const ProductionItem & pylon, bool tentative = false, int requiredWidth = 0)
        {
            auto & pylons = buildLocations[neighbourhood][2];
            if (pylons.empty()) return;

            // All else being equal, try to keep two of each location type powered
            int desiredMedium = std::max(0, 2 - (int)buildLocations[neighbourhood][3].size());
            int desiredLarge = std::max(0, 2 - (int)buildLocations[neighbourhood][4].size());

            // Score the locations based on distance and what they will power
            int bestScore = INT_MAX;
            const BuildingPlacement::BuildLocation * best = nullptr;
            for (auto it = pylons.begin(); it != pylons.end(); it++)
            {
                // If we have a hard requirement, don't consider anything that doesn't satisfy it
                if ((requiredWidth == 3 && it->powersMedium.empty()) ||
                    (requiredWidth == 4 && it->powersLarge.empty()))
                {
                    continue;
                }

                // Base score is the distance
                int score = it->builderFrames;

                // Penalize locations that don't power locations we need
                score *= (int)std::pow(2, std::max(0, desiredMedium - (int)it->powersMedium.size()));
                score *= (int)std::pow(2, std::max(0, desiredLarge - (int)it->powersLarge.size()));

                // Set if better than current
                if (score < bestScore)
                {
                    bestScore = score;
                    best = &*it;
                }
            }

            // If we found a match, use it
            if (best)
            {
                pylon.estimatedWorkerMovementTime = best->builderFrames;
                if (!tentative)
                {
                    pylon.buildLocation = *best;
                    pylons.erase(*best);
                }
                return;
            }

            // That failed, so we can't satisfy our hard requirement
            // Just return the first one
            // TODO: Handle this situation
            pylon.estimatedWorkerMovementTime = pylons.begin()->builderFrames;
            if (!tentative)
            {
                pylon.buildLocation = *pylons.begin();
                pylons.erase(pylons.begin());
            }
        }

        void reserveBuildPositions(ProductionItemSet & items, ProductionItemSet & committedItems)
        {
            for (auto it = items.begin(); it != items.end(); it++)
            {
                auto & item = **it;

                if (item.queuedBuilding) continue;
                if (item.buildLocation.tile.isValid()) continue;

                // Choose a tentative build location for pylons that don't already have one
                // It may be made concrete later to account for psi requirements
                if (item.is(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    choosePylonBuildLocation(BuildingPlacement::Neighbourhood::MainBase, item, true);
                    continue;
                }

                auto unitType = std::get_if<BWAPI::UnitType>(&item.type);

                if (!unitType->isBuilding()) continue;
                if (!unitType->requiresPsi()) continue;

                // TODO: Allow items to define their own neighbourhood
                auto neighbourhood = BuildingPlacement::Neighbourhood::MainBase;
                auto & locations = buildLocations[neighbourhood][unitType->tileWidth()];

                // Get the frame when the next available build location will be powered
                int availableAt = locations.empty() ? INT_MAX : locations.begin()->framesUntilPowered;

                // If there is an available location now, just take it
                if (availableAt <= item.startFrame)
                {
                    item.estimatedWorkerMovementTime = locations.begin()->builderFrames;
                    item.buildLocation = *locations.begin();
                    locations.erase(locations.begin());
                    continue;
                }

                // Find a committed pylon that has no assigned build location
                std::shared_ptr<ProductionItem> pylon = nullptr;
                for (auto & committedItem : committedItems)
                {
                    if (!committedItem->is(BWAPI::UnitTypes::Protoss_Pylon)) continue;
                    if (committedItem->queuedBuilding) continue;
                    if (committedItem->buildLocation.tile.isValid()) continue;

                    pylon = committedItem;
                    break;
                }

                // If there isn't one, queue it unless we can't beat our current availability frame
                if (!pylon && availableAt >= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    int startFrame = std::max(0, item.startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                    pylon = *items.emplace(std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Pylon, startFrame));
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
                    choosePylonBuildLocation(neighbourhood, *pylon, unitType->tileWidth());

                    // TODO: The powered after times are wrong here, as the pylon may get moved later
                    // Should perhaps look for available build locations within the committed pylon set instead
                    // Might make sense to move the build location stuff after minerals, etc. too
                    for (auto & buildLocation : pylon->buildLocation.powersMedium)
                    {
                        buildLocations[neighbourhood][3].emplace(buildLocation.tile, buildLocation.builderFrames, pylon->completionFrame);
                    }
                    for (auto & buildLocation : pylon->buildLocation.powersLarge)
                    {
                        buildLocations[neighbourhood][4].emplace(buildLocation.tile, buildLocation.builderFrames, pylon->completionFrame);
                    }
                }

                // TODO: Cancel everything if there are no build lcoations
                if (locations.empty()) return;

                // Take the best build location
                item.estimatedWorkerMovementTime = locations.begin()->builderFrames;
                item.buildLocation = *locations.begin();
                locations.erase(locations.begin());

                // If it is first powered later, shift the remaining items in the queue
                shiftAll(items, it, item.buildLocation.framesUntilPowered - item.startFrame);
            }
        }

        void insertProduction(ProductionItemSet & items, Type type, int frame, const std::shared_ptr<Producer> producer)
        {
            int build = buildTime(type);
            auto it = producer->items.begin();
            while (frame < PREDICT_FRAMES)
            {
                // If the producer already has queued items, make sure this item can be inserted
                if (it != producer->items.end() && frame >= ((*it)->startFrame - build))
                {
                    frame = (*it)->completionFrame;
                    continue;
                }

                items.emplace(std::make_shared<ProductionItem>(type, frame, producer));
                frame += build;
            }
        }

        void removeExcessProduction(ProductionItemSet & items, Type type, int & countToProduce)
        {
            if (countToProduce == -1) return;

            for (auto it = items.begin(); it != items.end(); )
            {
                if ((*it)->type != type)
                {
                    it++;
                    continue;
                }

                if (countToProduce > 0)
                {
                    countToProduce--;
                    it++;
                }
                else
                {
                    (*it)->producer->items.erase(*it);
                    it = items.erase(it);
                }
            }
        }

        // When prerequisites are involved, the start frames will initially be negative
        // This shifts all of the frames in the set forward so they start at 0
        void normalizeStartFrame(ProductionItemSet & items)
        {
            int offset = 0;
            bool first = true;
            for (auto & item : items)
            {
                if (first) offset = -item->startFrame, first = false;
                item->startFrame += offset;
                item->completionFrame += offset;
            }
        }

        bool shiftForMinerals(ProductionItemSet & goalItems, ProductionItemSet::iterator it)
        {
            // Find the frame where we have enough minerals from that point forward
            // We do this by finding the first frame scanning backwards where we don't have enough minerals
            int mineralCost = (*it)->mineralPrice();
            int f;
            for (f = PREDICT_FRAMES - 1; f >= (*it)->startFrame; f--)
            {
                if (minerals[f] < mineralCost) break;
            }
            f++;

            // Workers are a special case, as they start producing income after they are completed
            // So for them we just need enough minerals to cover the period until they have recovered their investment
            if (f > (*it)->startFrame && (*it)->is(BWAPI::UnitTypes::Protoss_Probe))
            {
                f = (*it)->startFrame;
                int timeToBuild = buildTime((*it)->type);
                for (int i = (*it)->startFrame; i < PREDICT_FRAMES; i++)
                {
                    int requiredMinerals = mineralCost;
                    int miningTime = i - f - timeToBuild;
                    if (miningTime > 0) requiredMinerals -= (int)((double)miningTime * MINERALS_PER_WORKER_FRAME);
                    if (requiredMinerals <= 0) break;
                    if (minerals[i] < requiredMinerals) f = i;
                }
            }

            // If we can't ever produce this item, return false
            // Caller will cancel the remaining items
            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            // If we can't produce the item immediately, shift the start frame for this and all remaining items
            // Since we are preserving the relative order, we don't need to reinsert anything into the set
            shiftAll(goalItems, it, f - (*it)->startFrame);

            return true;
        }

        bool shiftForGas(ProductionItemSet & goalItems, ProductionItemSet & committedItems, ProductionItemSet::iterator it)
        {
            if ((*it)->gasPrice() == 0) return true;

            // Find how much gas we are missing to produce the item now
            int gasCost = (*it)->gasPrice();
            int gasDeficit = 0;
            for (int f = PREDICT_FRAMES - 1; f >= (*it)->startFrame; f--)
            {
                gasDeficit = std::max(gasDeficit, gasCost - gas[f]);
            }

            // If we have enough gas now, just return
            if (gasDeficit == 0) return true;

            // We are gas blocked, so there are three options:
            // - Shift one or more queued refineries earlier
            // - Add a refinery
            // - Shift the start frame of this item to where we have gas
            // TODO: Also adjust number of gas workers

            auto refineryType = BWAPI::Broodwar->self()->getRace().getRefinery();

            // Compute how many frames of 3 workers collecting gas is needed to produce the item
            int gasFramesNeeded = (int)((double)gasDeficit / (GAS_PER_WORKER_FRAME * 3));

            // Look for refineries we can move earlier
            for (auto refineryIt = committedItems.begin(); gasFramesNeeded > 0 && refineryIt != committedItems.end();)
            {
                if (!(*refineryIt)->is(refineryType) || (*refineryIt)->queuedBuilding)
                {
                    refineryIt++;
                    continue;
                }
                
                // What frame do we want to move this refinery back to?
                int desiredStartFrame = std::max(0, (*it)->startFrame - gasFramesNeeded - UnitUtil::BuildTime(refineryType));

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
                    int mineralsNeeded = (int)(3.0 * (f - actualStartFrame) * MINERALS_PER_WORKER_FRAME);
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

                    // Reduce the needed gas frames if it helps the current item
                    if ((*refineryIt)->completionFrame < (*it)->startFrame)
                        gasFramesNeeded -= delta;
                    else if (completionFrame < (*it)->startFrame)
                        gasFramesNeeded -= (*it)->startFrame - completionFrame;

                    shiftOne(committedItems, *refineryIt, -delta);

                    // Restart the iteration as the iterator may have been invalidated
                    if (gasFramesNeeded > 0) refineryIt = committedItems.begin();
                }
                else
                    refineryIt++;
            }

            // If we've resolved the gas block, return
            if (gasFramesNeeded == 0) return true;

            // Add a refinery if possible
            if (!availableGeysers.empty())
            {
                // Ideal timing makes enough gas available to build the current item
                int desiredStartFrame = std::max(0, (*it)->startFrame - gasFramesNeeded - UnitUtil::BuildTime(refineryType));

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
                    int mineralsNeeded = refineryType.mineralPrice() + (int)(3.0 * (f - actualStartFrame) * MINERALS_PER_WORKER_FRAME);
                    if (minerals[completionFrame] < mineralsNeeded)
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }
                }

                // Queue it if it was possible to build
                if (actualStartFrame < PREDICT_FRAMES)
                {
                    // Spend minerals
                    spendResource(minerals, actualStartFrame, refineryType.mineralPrice());

                    // Update mineral and gas collection
                    updateResourceCollection(minerals, actualStartFrame + UnitUtil::BuildTime(refineryType), -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, actualStartFrame + UnitUtil::BuildTime(refineryType), 3, GAS_PER_WORKER_FRAME);

                    // Commit
                    // TODO: Consider / determine the queue time
                    auto geyser = *committedItems.emplace(std::make_shared<ProductionItem>(refineryType, actualStartFrame));
                    geyser->buildLocation = *availableGeysers.begin();
                    availableGeysers.erase(availableGeysers.begin());
                }
            }
                       
            // Shift the item to when we have the gas for it
            int f;
            for (f = PREDICT_FRAMES - 1; f >= (*it)->startFrame; f--)
            {
                if (gas[f] < gasCost) break;
            }
            f++;

            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            shiftAll(goalItems, it, f - (*it)->startFrame);

            return true;
        }

        bool shiftForSupply(ProductionItemSet & goalItems, ProductionItemSet & committedItems, ProductionItemSet::iterator it)
        {
            if ((*it)->supplyRequired() == 0) return true;

            // TODO: Consider keeping a changelog so we can roll back if an unresolvable block appears after we
            // have already moved something

            for (int f = (*it)->startFrame; f < PREDICT_FRAMES; f++)
            {
            startFrame:
                if (supply[f] >= (*it)->supplyRequired()) continue;

                // There is a supply block at this frame, let's try to resolve it

                // Look for an existing supply provider active at this time
                std::shared_ptr<ProductionItem> supplyProvider = nullptr;
                for (auto & potentialSupplyProvider : committedItems)
                {
                    // Ignore anything that doesn't provide supply or completes before the block
                    if (potentialSupplyProvider->supplyProvided() <= 0) continue;
                    if (potentialSupplyProvider->completionFrame <= f) continue;

                    // If a supply provider is already being built, push the item until the completion frame
                    // TODO: This probably isn't a valid approach for queued nexuses as we might be able to fit in a pylon ahead of time
                    if (potentialSupplyProvider->queuedBuilding)
                    {
                        f = potentialSupplyProvider->completionFrame;
                        if (f >= PREDICT_FRAMES) return false;

                        shiftAll(goalItems, it, potentialSupplyProvider->completionFrame - (*it)->startFrame);
                        goto startFrame;
                    }

                    supplyProvider = potentialSupplyProvider;
                    break;
                }

                // If there is one, attempt to move it earlier
                if (supplyProvider)
                {
                    // Try to shift the supply provider so that it completes at the time of the block
                    int desiredStartFrame = std::max(0, f - buildTime(supplyProvider->type));

                    // Determine how far we can move the supply provider back and still have minerals
                    int mineralsNeeded = supplyProvider->mineralPrice() + (*it)->mineralPrice();
                    int mineralFrame;
                    for (mineralFrame = supplyProvider->startFrame - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                    {
                        if (minerals[mineralFrame] < mineralsNeeded) break;
                        if (mineralFrame == (*it)->startFrame) mineralsNeeded -= (*it)->mineralPrice();
                    }
                    mineralFrame++;

                    // If we could move it back, make the relevant adjustments
                    shiftOne(committedItems, supplyProvider, mineralFrame - supplyProvider->startFrame);

                    // If the block is resolved, continue the main loop
                    if (supply[f] >= (*it)->supplyRequired()) continue;

                    // Otherwise shift the item to the next frame where we have supply for it
                    for (; f < PREDICT_FRAMES; f++)
                    {
                        if (supply[f] >= (*it)->supplyRequired()) break;
                    }
                    if (f == PREDICT_FRAMES) return false;
                    shiftAll(goalItems, it, f - (*it)->startFrame);
                    
                    // Continue and see if we created another supply block later
                    continue;
                }

                // There was no active supply provider to move
                // Let's see when we have the resources to queue a new one

                // Find the frame when we have enough minerals to produce the pylon
                int desiredStartFrame = std::max(0, f - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                int mineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() + (*it)->mineralPrice();
                int mineralFrame;
                for (mineralFrame = PREDICT_FRAMES - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                {
                    if (minerals[mineralFrame] < mineralsNeeded) break;
                    if (mineralFrame == (*it)->startFrame) mineralsNeeded -= (*it)->mineralPrice();
                }
                mineralFrame++;

                if (mineralFrame == PREDICT_FRAMES) return false;

                // Detect the case where the supply block blocks the current item and it is better to delay the previous item
                // to resolve the supply block earlier
                if (f == (*it)->startFrame && mineralFrame != desiredStartFrame && it != goalItems.begin())
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
                            f = (*it)->startFrame;

                            // Pylon is now being produced at the new frame we found
                            mineralFrame = pylonMineralFrame;
                        }
                    }
                }

                // Queue the supply provider
                committedItems.emplace(std::make_shared<ProductionItem>(BWAPI::UnitTypes::Protoss_Pylon, mineralFrame));
                spendResource(supply, mineralFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon), -BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());
                spendResource(minerals, mineralFrame, BWAPI::UnitTypes::Protoss_Pylon.mineralPrice());

                // If the block is resolved, continue the main loop
                if (mineralFrame == desiredStartFrame) continue;

                // Otherwise shift the item to the next frame where we have supply for it
                for (; f < PREDICT_FRAMES; f++)
                {
                    if (supply[f] >= (*it)->supplyRequired()) break;
                }
                if (f == PREDICT_FRAMES) return false;
                shiftAll(goalItems, it, f - (*it)->startFrame);
            }

            return true;
        }

        void resolveResourceBlocksAndCommit(ProductionItemSet & goalItems, ProductionItemSet & committedItems)
        {
            bool cancelAll = false;
            for (auto it = goalItems.begin(); it != goalItems.end(); )
            {
                auto & item = **it;

                // Remove items beyond frame PREDICT_FRAMES or if we couldn't afford an earlier item
                if (cancelAll || item.startFrame >= PREDICT_FRAMES)
                {
                    if (item.producer) item.producer->items.erase(*it);
                    it = goalItems.erase(it);
                    continue;
                }

                // Shift for each resource type or cancel if it can't be produced
                if (!shiftForMinerals(goalItems, it) ||
                    !shiftForGas(goalItems, committedItems, it) ||
                    !shiftForSupply(goalItems, committedItems, it))
                {
                    cancelAll = true;
                    continue;
                }

                // Spend the resources
                spendResource(minerals, item.startFrame, item.mineralPrice());
                if (item.gasPrice() > 0) spendResource(gas, item.startFrame, item.gasPrice());
                if (item.supplyRequired() > 0) spendResource(supply, item.startFrame, item.supplyRequired());

                // If the item is a worker, assume it will go on minerals when complete
                if (item.is(BWAPI::Broodwar->self()->getRace().getWorker()))
                    updateResourceCollection(minerals, item.completionFrame, 1, MINERALS_PER_WORKER_FRAME);

                // If the item is a refinery, move three workers to it when it finishes
                // TODO: allow more fine-grained selection of gas workers
                if (item.is(BWAPI::Broodwar->self()->getRace().getRefinery()))
                {
                    updateResourceCollection(minerals, item.completionFrame, -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, item.completionFrame, 3, GAS_PER_WORKER_FRAME);
                }

                // If the item provides supply, add it
                if (item.supplyProvided() > 0)
                    spendResource(supply, item.startFrame, -item.supplyProvided());

                // We commit refineries and supply providers immediately
                // This helps to simplify the reordering logic in shiftForGas and shiftForSupply
                if (item.is(BWAPI::Broodwar->self()->getRace().getRefinery()) || item.supplyProvided() > 0)
                {
                    committedItems.insert(*it);
                    it = goalItems.erase(it);
                }
                else
                    it++;
            }

            // Commit remaining goal items
            for (auto it = goalItems.begin(); it != goalItems.end(); )
            {
                // Remove producers that have no items to produce
                if (!(*it)->isPrerequisite && queuedProducers[*it] && queuedProducers[*it]->items.empty())
                {
                    spendResource(minerals, (*it)->startFrame, -(*it)->mineralPrice());
                    spendResource(gas, (*it)->startFrame, -(*it)->gasPrice());
                    it = goalItems.erase(it);
                    continue;
                }

                // Otherwise commit
                committedItems.insert(*it);
                it++;
            }
        }

        // Goal handler for unit production
        void handleGoal(ProductionItemSet & committedItems, Type type, int countToProduce, int producerLimit, BWAPI::UnitType prerequisite = BWAPI::UnitTypes::None)
        {
            int producerCount = 0;

            // For some logic we need to know which variant type we are producing, so reference them here
            auto unitType = std::get_if<BWAPI::UnitType>(&type);
            auto upgradeType = std::get_if<BWAPI::UpgradeType>(&type);

            BWAPI::UnitType producerType;
            if (unitType)
                producerType = unitType->whatBuilds().first;
            else if (upgradeType)
                producerType = upgradeType->whatUpgrades();

            ProductionItemSet goalItems;

            // Step 1: Ensure we have all buildings we need to build this type

            // Add missing prerequisites for the type
            if (unitType) addMissingPrerequisites(goalItems, *unitType);
            if (prerequisite != BWAPI::UnitTypes::None) addBuildingIfIncomplete(goalItems, prerequisite, 0, true);

            // Unless this item is a building, ensure we have something that can produce it
            if (!unitType || !unitType->isBuilding()) addBuildingIfIncomplete(goalItems, producerType);

            // Shift the buildings so the frames start at 0
            normalizeStartFrame(goalItems);

            // Resolve duplicates, keeping the earliest one of each type
            resolveDuplicates(goalItems, committedItems);

            // Reserve positions for all buildings
            reserveBuildPositions(goalItems, committedItems);

            // Record the frame when prerequisites are available
            int prerequisitesAvailable = goalItems.empty() ? 0 : (*goalItems.rbegin())->completionFrame;

            // Step 2: Insert production from all existing or planned producers

            // Insert production for planned producers for this goal
            for (auto & item : goalItems)
                if (item->is(producerType))
                {
                    if (!queuedProducers[item]) queuedProducers[item] = std::make_shared<Producer>(item);
                    insertProduction(goalItems, type, std::max(prerequisitesAvailable, item->completionFrame), queuedProducers[item]);
                    producerCount++;
                }

            // Insert production for committed producers
            for (auto & item : committedItems)
                if (item->is(producerType))
                {
                    if (!queuedProducers[item]) queuedProducers[item] = std::make_shared<Producer>(item);
                    insertProduction(goalItems, type, std::max(prerequisitesAvailable, item->completionFrame), queuedProducers[item]);
                    producerCount++;
                }

            // Insert production for completed producers
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->exists() || !unit->isCompleted()) continue;
                if (unit->getType() != producerType) continue;

                int remainingTrainTime = 0;
                if (unitType)
                {
                    remainingTrainTime = unit->getRemainingTrainTime();
                    if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Train &&
                        (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame()) <= BWAPI::Broodwar->getLatencyFrames())
                    {
                        remainingTrainTime = UnitUtil::BuildTime(unit->getLastCommand().getUnitType());
                    }
                }
                else if (upgradeType)
                {
                    remainingTrainTime = unit->getRemainingUpgradeTime();
                    if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Upgrade &&
                        (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame()) <= BWAPI::Broodwar->getLatencyFrames())
                    {
                        remainingTrainTime = buildTime(unit->getLastCommand().getUpgradeType());
                    }
                }

                if (!existingProducers[unit]) existingProducers[unit] = std::make_shared<Producer>(unit);
                insertProduction(goalItems, type, std::max(prerequisitesAvailable, remainingTrainTime), existingProducers[unit]);
                producerCount++;
            }

            // Remove any excess unit production
            removeExcessProduction(goalItems, type, countToProduce);

            // Step 3: Turn our idealized timings into real timings by resolving any resource blocks
            // This will either push or cancel items that we don't have minerals, gas, or supply for,
            // if those problems cannot be resolved
            resolveResourceBlocksAndCommit(goalItems, committedItems);

            // Step 4: If the goal wants to add producers, add as many as possible
            // We break when the previous iteration could not commit anything
            while ((countToProduce == -1 || countToProduce > 0) && (producerLimit == -1 || producerCount < producerLimit) && !goalItems.empty())
            {
                goalItems.clear();

                // This just repeats most of the above steps but only considering adding one producer
                int startFrame = std::max(0, prerequisitesAvailable - UnitUtil::BuildTime(producerType));
                auto producer = *goalItems.emplace(std::make_shared<ProductionItem>(producerType, startFrame));
                producerCount++;

                reserveBuildPositions(goalItems, committedItems);
                insertProduction(goalItems, type, producer->completionFrame, std::make_shared<Producer>(producer));
                removeExcessProduction(goalItems, type, countToProduce);
                resolveResourceBlocksAndCommit(goalItems, committedItems);
            }
        }
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // The overall objective is, for each production goal, to determine what should be produced next and
        // do so whenever this does not delay a higher-priority goal.

        ProductionItemSet committedItems;

        initializeResources(committedItems);

        for (auto goal : Strategist::currentProductionGoals())
        {
            if (auto unitProductionGoal = std::get_if<UnitProductionGoal>(&goal))
            {
                if (unitProductionGoal->isFulfilled()) continue;
                handleGoal(committedItems, unitProductionGoal->unitType(), unitProductionGoal->countToProduce(), unitProductionGoal->getProducerLimit());
            }
            else if (auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&goal))
            {
                if (upgradeProductionGoal->isFulfilled()) continue;
                handleGoal(committedItems, upgradeProductionGoal->upgradeType(), 1, 1, upgradeProductionGoal->prerequisiteForNextLevel());
            }
            else
            {
                Log::Get() << "ERROR: Unknown variant type for ProductionGoal";
            }
        }

        write(committedItems);

        // Issue build commands
        for (auto & item : committedItems)
        {
            // Units
            if (auto unitType = std::get_if<BWAPI::UnitType>(&item->type))
            {
                // Produce units desired at frame 0
                if (item->producer && item->producer->existing && item->startFrame == 0)
                {
                    item->producer->existing->train(*unitType);
                }

                // Pylons may not already have a build location
                if (item->is(BWAPI::UnitTypes::Protoss_Pylon) && !item->buildLocation.tile.isValid())
                {
                    // TODO: Allow items to specify their neighbourhood
                    auto neighbourhood = BuildingPlacement::Neighbourhood::MainBase;

                    choosePylonBuildLocation(neighbourhood, *item);
                }

                // Queue buildings when we expect the builder will arrive at the desired start frame or later
                if (unitType->isBuilding() && !item->queuedBuilding &&
                    (item->startFrame - item->buildLocation.builderFrames) <= 0)
                {
                    // TODO: Should be resolved earlier
                    if (!item->buildLocation.tile.isValid()) continue;

                    int arrivalFrame;
                    auto builder = Builder::getBuilderUnit(item->buildLocation.tile, *unitType, &arrivalFrame);
                    if (builder && arrivalFrame >= item->startFrame)
                    {
                        Builder::build(*unitType, item->buildLocation.tile, builder);

                        // TODO: This needs to be handled better
                        if (*unitType == BWAPI::Broodwar->self()->getRace().getRefinery())
                        {
                            Workers::setDesiredGasWorkers(Workers::gasWorkers() + 3);
                        }
                    }
                }
            }

            // Upgrades
            else if (auto upgradeType = std::get_if<BWAPI::UpgradeType>(&item->type))
            {
                // Upgrade if start frame is now
                if (item->producer && item->producer->existing && item->startFrame == 0)
                {
                    item->producer->existing->upgrade(*upgradeType);
                }
            }
        }
    }
}
