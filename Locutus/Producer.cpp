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
- Support building a specific number of units as quickly as possible
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
        const int PREDICT_FRAMES = 4000;

        const double MINERALS_PER_WORKER_FRAME = 0.0456;
        const double GAS_PER_WORKER_FRAME = 0.071;

        std::array<int, PREDICT_FRAMES> minerals;
        std::array<int, PREDICT_FRAMES> gas;
        std::array<int, PREDICT_FRAMES> supply;
        std::map<BuildingPlacement::Neighbourhood, std::map<int, BuildingPlacement::BuildLocationSet>> buildLocations;
        BuildingPlacement::BuildLocationSet availableGeysers;

        const BuildingPlacement::BuildLocation InvalidBuildLocation(BWAPI::TilePositions::Invalid, 0, 0);

        // Helper type to store what we need to produce when
        struct ProductionItem
        {
            struct cmp
            {
                bool operator()(const ProductionItem & a, const ProductionItem & b) const
                {
                    return a.startFrame < b.startFrame 
                        || (!(b.startFrame < a.startFrame) && a.completionFrame < b.completionFrame)
                        || (!(b.startFrame < a.startFrame) && !(b.completionFrame < a.completionFrame) && a.type < b.type);
                }
            };

            BWAPI::UnitType                          type;
            mutable int                              startFrame;
            mutable int                              completionFrame;
            Building *                               queuedBuilding;
            const ProductionItem *                   queuedProducer;
            BWAPI::Unit                              existingProducer;
            mutable BuildingPlacement::BuildLocation buildLocation;

            ProductionItem(BWAPI::UnitType type, int startFrame, const ProductionItem * queuedProducer = nullptr, BWAPI::Unit existingProducer = nullptr)
                : type(type)
                , startFrame(startFrame)
                , completionFrame(startFrame + UnitUtil::BuildTime(type))
                , queuedBuilding(nullptr)
                , queuedProducer(queuedProducer)
                , existingProducer(existingProducer)
                , buildLocation(InvalidBuildLocation)
            {}

            // Constructor for pending buildings
            ProductionItem(Building* queuedBuilding)
                : type(queuedBuilding->type)
                , startFrame(queuedBuilding->expectedFramesUntilStarted())
                , completionFrame(queuedBuilding->expectedFramesUntilCompletion())
                , queuedBuilding(queuedBuilding)
                , queuedProducer(nullptr)
                , existingProducer(nullptr)
                , buildLocation(InvalidBuildLocation)
            {}

            // Constructor for when we are moving an item to a different frame
            ProductionItem(const ProductionItem & other, int delta)
                : type(other.type)
                , startFrame(other.startFrame + delta)
                , completionFrame(other.completionFrame + delta)
                , queuedBuilding(other.queuedBuilding)
                , queuedProducer(other.queuedProducer)
                , existingProducer(other.existingProducer)
                , buildLocation(other.buildLocation)
            {}
        };

        typedef std::multiset<ProductionItem, ProductionItem::cmp> ProductionItemSet;

        void write(ProductionItemSet & items)
        {
            auto itemLabel = [](const ProductionItem & item)
            {
                std::ostringstream label;
                label << item.type;
                if (item.queuedBuilding && item.queuedBuilding->isConstructionStarted()) label << "^";
                if (item.queuedBuilding && !item.queuedBuilding->isConstructionStarted()) label << "*";
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
                    if (item.startFrame >= PREDICT_FRAMES) continue;
                    csv << itemLabel(item) << item.startFrame << item.completionFrame;
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
                    if (item.startFrame >= PREDICT_FRAMES) continue;
                    csv << minerals[item.startFrame] << gas[item.startFrame] << supply[item.startFrame];
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
                auto item = committedItems.emplace(pendingBuilding);
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
        }

        // Need a forward reference since the next two methods do mutual recursion
        void addMissingPrerequisites(ProductionItemSet & items, BWAPI::UnitType unitType, int startFrame = 0);

        void addBuildingIfIncomplete(ProductionItemSet & items, BWAPI::UnitType unitType, int baseFrame = 0)
        {
            if (Units::countCompleted(unitType) > 0) return;

            int startFrame = baseFrame - UnitUtil::BuildTime(unitType);
            items.emplace(unitType, startFrame);
            addMissingPrerequisites(items, unitType, startFrame);
        }

        // For the specified unit type, recursively checks what prerequisites are missing and adds them to the set
        // The frames are relative to the time when the first unit of the desired type can be produced
        void addMissingPrerequisites(ProductionItemSet & items, BWAPI::UnitType unitType, int startFrame)
        {
            for (auto typeAndCount : unitType.requiredUnits())
            {
                // Don't include the whatBuilds item here, we handle them separately
                if (typeAndCount.first == unitType.whatBuilds().first) continue;

                addBuildingIfIncomplete(items, typeAndCount.first, startFrame);
            }
        }

        // Shifts the item pointed to by the iterator and all future items by the given number of frames
        void shift(ProductionItemSet & items, ProductionItemSet::iterator it, int delta)
        {
            if (delta < 1 || it == items.end()) return;

            int startFrame = it->startFrame;

            for (; it != items.end(); it++)
            {
                it->startFrame += delta;
                it->completionFrame += delta;

                // If this item has a queued producer attached, we should shift it too
                // TODO: Should we extend this to also apply to prerequisite buildings? Would require a list/set and recursion
                if (it->queuedProducer && !it->queuedProducer->queuedBuilding && it->queuedProducer->startFrame < startFrame)
                {
                    spendResource(minerals, it->queuedProducer->startFrame, -it->queuedProducer->type.mineralPrice());
                    spendResource(gas, it->queuedProducer->startFrame, -it->queuedProducer->type.gasPrice());

                    // Technically changing this could put the set out of order
                    // However we know that we do not iterate the set again after this point, so it's OK to ignore that
                    it->queuedProducer->startFrame += delta;
                    it->queuedProducer->completionFrame += delta;

                    spendResource(minerals, it->queuedProducer->startFrame, it->queuedProducer->type.mineralPrice());
                    spendResource(gas, it->queuedProducer->startFrame, it->queuedProducer->type.gasPrice());
                }
            }
        }

        // Remove duplicate items from this goal's set and resolve conflicts with already-committed items
        void resolveDuplicates(ProductionItemSet & items, ProductionItemSet & committedItems)
        {
            std::set<BWAPI::UnitType> seen;
            for (auto it = items.begin(); it != items.end(); )
            {
                // There's already an item of this type in this goal's queue
                if (seen.find(it->type) != seen.end())
                {
                    it = items.erase(it);
                    continue;
                }

                // Check if there is a matching item in the committed item set
                bool handled = false;
                for (auto & committedItem : committedItems)
                {
                    if (committedItem.type != it->type) continue;

                    // We found a match

                    // Compute the difference in completion times
                    int delta = committedItem.completionFrame - it->completionFrame;

                    // Remove the item
                    it = items.erase(it);

                    // If the committed item completes later, shift all the later items in this goal's queue
                    // The rationale for this is that the committed item set is already optimized to produce everything
                    // as early as possible, so we know we can't move it earlier
                    shift(items, it, delta);

                    handled = true;
                    break;
                }

                // We didn't find a match, so just register this type and continue
                if (!handled)
                {
                    seen.insert(it->type);
                    it++;
                }
            }
        }

        void choosePylonBuildLocation(BuildingPlacement::Neighbourhood neighbourhood, const ProductionItem & pylon, int requiredWidth = 0)
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
                pylon.buildLocation = *best;
                pylons.erase(*best);
                return;
            }

            // That failed, so we can't satisfy our hard requirement
            // Just return the first one
            // TODO: Handle this situation
            pylon.buildLocation = *pylons.begin();
            pylons.erase(pylons.begin());
        }

        void reserveBuildPositions(ProductionItemSet & items, ProductionItemSet & committedItems)
        {
            for (auto it = items.begin(); it != items.end(); it++)
            {
                if (it->queuedBuilding) continue;
                if (!it->type.isBuilding()) continue;
                if (!it->type.requiresPsi()) continue;

                // TODO: Allow items to define their own neighbourhood
                auto neighbourhood = BuildingPlacement::Neighbourhood::MainBase;
                auto & locations = buildLocations[neighbourhood][it->type.tileWidth()];

                // Get the frame when the next available build location will be powered
                int availableAt = locations.empty() ? INT_MAX : locations.begin()->framesUntilPowered;

                // If there is an available location now, just take it
                if (availableAt <= it->startFrame)
                {
                    it->buildLocation = *locations.begin();
                    locations.erase(locations.begin());
                    continue;
                }

                // Find a committed pylon that has no assigned build location
                const ProductionItem * pylon = nullptr;
                for (auto & committedItem : committedItems)
                {
                    if (committedItem.type != BWAPI::UnitTypes::Protoss_Pylon) continue;
                    if (committedItem.buildLocation.tile.isValid()) continue;

                    pylon = &committedItem;
                    break;
                }

                // If there isn't one, queue it unless we can't beat our current availability frame
                if (!pylon && availableAt >= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon))
                {
                    int startFrame = std::max(0, it->startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                    auto result = items.emplace(BWAPI::UnitTypes::Protoss_Pylon, startFrame);
                    pylon = &*result;
                }

                // If the pylon will complete later than we need it, try to move it earlier
                if (pylon && pylon->completionFrame > it->startFrame && pylon->startFrame > 0)
                {
                    // Try to shift the pylon so that it completes right when we need it
                    int desiredStartFrame = std::max(0, it->startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));

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
                        int delta = pylon->startFrame - mineralFrame;
                        if (delta > 0)
                        {
                            spendResource(supply, pylon->completionFrame, BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());
                            spendResource(supply, mineralFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon), -BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());

                            spendResource(minerals, pylon->startFrame, -BWAPI::UnitTypes::Protoss_Pylon.mineralPrice());
                            spendResource(minerals, mineralFrame, BWAPI::UnitTypes::Protoss_Pylon.mineralPrice());

                            // We reinsert it so it gets sorted correctly
                            auto result = committedItems.emplace(*pylon, -delta);
                            committedItems.erase(*pylon);
                            pylon = &*result;
                        }
                    }
                }

                // If we are creating or moving a pylon, pick a location and add the new build locations
                if (pylon)
                {
                    choosePylonBuildLocation(neighbourhood, *pylon, it->type.tileWidth());

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
                it->buildLocation = *locations.begin();
                locations.erase(locations.begin());

                // If it is first powered later, shift the remaining items in the queue
                shift(items, it, it->buildLocation.framesUntilPowered - it->startFrame);
            }
        }

        void insertProduction(ProductionItemSet & items, BWAPI::UnitType unitType, int frame, const ProductionItem * queuedProducer = nullptr, BWAPI::Unit existingProducer = nullptr)
        {
            bool first = true;
            while (frame < PREDICT_FRAMES)
            {
                items.emplace(unitType, frame, first ? queuedProducer : nullptr, existingProducer);
                frame += UnitUtil::BuildTime(unitType);
                first = false;
            }
        }
        void insertProduction(ProductionItemSet & items, BWAPI::UnitType unitType, int frame, BWAPI::Unit existingProducer)
        {
            insertProduction(items, unitType, frame, nullptr, existingProducer);
        }

        void removeExcessProduction(ProductionItemSet & items, BWAPI::UnitType unitType, int & countToProduce)
        {
            if (countToProduce == -1) return;

            for (auto it = items.begin(); it != items.end(); )
            {
                if (it->type != unitType)
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
                    it = items.erase(it);
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
                if (first) offset = -item.startFrame, first = false;
                item.startFrame += offset;
                item.completionFrame += offset;
            }
        }

        bool shiftForMinerals(ProductionItemSet & goalItems, ProductionItemSet::iterator it)
        {
            // Find the frame where we have enough minerals from that point forward
            // We do this by finding the first frame scanning backwards where we don't have enough minerals
            int mineralCost = it->type.mineralPrice();
            int f;
            for (f = PREDICT_FRAMES - 1; f >= it->startFrame; f--)
            {
                if (minerals[f] < mineralCost) break;
            }
            f++;

            // If we can't ever produce this item, return false
            // Caller will cancel the remaining items
            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            // If we can't produce the item immediately, shift the start frame for this and all remaining items
            // Since we are preserving the relative order, we don't need to reinsert anything into the set
            shift(goalItems, it, f - it->startFrame);

            return true;
        }

        bool shiftForGas(ProductionItemSet & goalItems, ProductionItemSet & committedItems, ProductionItemSet::iterator it)
        {
            if (it->type.gasPrice() == 0) return true;

            // Find how much gas we are missing to produce the item now
            int gasCost = it->type.gasPrice();
            int gasDeficit = 0;
            for (int f = PREDICT_FRAMES - 1; f >= it->startFrame; f--)
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
                if (refineryIt->type != refineryType || refineryIt->queuedBuilding)
                {
                    refineryIt++;
                    continue;
                }
                
                // What frame do we want to move this refinery back to?
                int desiredStartFrame = std::max(0, it->startFrame - gasFramesNeeded - UnitUtil::BuildTime(refineryType));

                // Find the actual frame we can move it to
                // We check two things:
                // - We have enough minerals to start the refinery at its new start frame
                // - We have enough minerals after the refinery completes and workers are reassigned
                int actualStartFrame = desiredStartFrame;
                for (int f = desiredStartFrame; f < refineryIt->startFrame && f < PREDICT_FRAMES - UnitUtil::BuildTime(refineryType); f++)
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
                    if (completionFrame < refineryIt->startFrame) mineralsNeeded += refineryType.mineralPrice();
                    if (minerals[completionFrame] < mineralsNeeded)
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }
                }

                // If we could move it back, make the relevant adjustment
                int delta = refineryIt->startFrame - actualStartFrame;
                if (delta > 0)
                {
                    int completionFrame = actualStartFrame + UnitUtil::BuildTime(refineryType);

                    // Reduce the needed gas frames if it helps the current item
                    if (refineryIt->completionFrame < it->startFrame)
                        gasFramesNeeded -= delta;
                    else if (completionFrame < it->startFrame)
                        gasFramesNeeded -= it->startFrame - completionFrame;

                    // Minerals needed to build
                    spendResource(minerals, refineryIt->startFrame, -refineryType.mineralPrice());
                    spendResource(minerals, actualStartFrame, refineryType.mineralPrice());

                    // Mineral and gas collection
                    updateResourceCollection(minerals, refineryIt->completionFrame, 3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, refineryIt->completionFrame, -3, GAS_PER_WORKER_FRAME);
                    updateResourceCollection(minerals, actualStartFrame + UnitUtil::BuildTime(refineryType), -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, actualStartFrame + UnitUtil::BuildTime(refineryType), 3, GAS_PER_WORKER_FRAME);

                    // We reinsert it so it gets sorted correctly
                    committedItems.emplace(*refineryIt, -delta);
                    refineryIt = committedItems.erase(refineryIt);
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
                int desiredStartFrame = std::max(0, it->startFrame - gasFramesNeeded - UnitUtil::BuildTime(refineryType));

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
                    auto geyser = committedItems.emplace(refineryType, actualStartFrame);
                    geyser->buildLocation = *availableGeysers.begin();
                    availableGeysers.erase(availableGeysers.begin());
                }
            }
                       
            // Shift the item to when we have the gas for it
            int f;
            for (f = PREDICT_FRAMES - 1; f >= it->startFrame; f--)
            {
                if (gas[f] < gasCost) break;
            }
            f++;

            if (f == PREDICT_FRAMES)
            {
                return false;
            }

            shift(goalItems, it, f - it->startFrame);

            return true;
        }

        bool shiftForSupply(ProductionItemSet & goalItems, ProductionItemSet & committedItems, ProductionItemSet::iterator it)
        {
            if (it->type.supplyRequired() == 0) return true;

            // TODO: Consider keeping a changelog so we can roll back if an unresolvable block appears after we
            // have already moved something

            for (int f = it->startFrame; f < PREDICT_FRAMES; f++)
            {
            startFrame:
                if (supply[f] >= it->type.supplyRequired()) continue;

                // There is a supply block at this frame, let's try to resolve it

                // Look for an existing supply provider active at this time
                const ProductionItem * supplyProvider = nullptr;
                for (auto & potentialSupplyProvider : committedItems)
                {
                    // Ignore anything that doesn't provide supply or completes before the block
                    if (potentialSupplyProvider.type.supplyProvided() <= 0) continue;
                    if (potentialSupplyProvider.completionFrame <= f) continue;

                    // If a supply provider is already being built, push the item until the completion frame
                    // TODO: This probably isn't a valid approach for queued nexuses as we might be able to fit in a pylon ahead of time
                    if (potentialSupplyProvider.queuedBuilding)
                    {
                        f = potentialSupplyProvider.completionFrame;
                        if (f >= PREDICT_FRAMES) return false;

                        shift(goalItems, it, potentialSupplyProvider.completionFrame - it->startFrame);
                        goto startFrame;
                    }

                    supplyProvider = &potentialSupplyProvider;
                    break;
                }

                // If there is one, attempt to move it earlier
                if (supplyProvider)
                {
                    // Try to shift the supply provider so that it completes at the time of the block
                    int desiredStartFrame = std::max(0, f - UnitUtil::BuildTime(supplyProvider->type));

                    // Determine how far we can move the supply provider back and still have minerals
                    int mineralsNeeded = supplyProvider->type.mineralPrice() + it->type.mineralPrice();
                    int mineralFrame;
                    for (mineralFrame = supplyProvider->startFrame - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                    {
                        if (minerals[mineralFrame] < mineralsNeeded) break;
                        if (mineralFrame == it->startFrame) mineralsNeeded -= it->type.mineralPrice();
                    }
                    mineralFrame++;

                    // If we could move it back, make the relevant adjustments
                    int delta = supplyProvider->startFrame - mineralFrame;
                    if (delta > 0)
                    {
                        spendResource(supply, supplyProvider->completionFrame, supplyProvider->type.supplyProvided());
                        spendResource(supply, mineralFrame + UnitUtil::BuildTime(supplyProvider->type), -supplyProvider->type.supplyProvided());

                        spendResource(minerals, supplyProvider->startFrame, -supplyProvider->type.mineralPrice());
                        spendResource(minerals, mineralFrame, supplyProvider->type.mineralPrice());

                        // We reinsert it so it gets sorted correctly
                        committedItems.emplace(*supplyProvider, -delta);
                        committedItems.erase(*supplyProvider);
                    }

                    // If the block is resolved, continue the main loop
                    if (supply[f] >= it->type.supplyRequired()) continue;

                    // Otherwise shift the item to the next frame where we have supply for it
                    for (; f < PREDICT_FRAMES; f++)
                    {
                        if (supply[f] >= it->type.supplyRequired()) break;
                    }
                    if (f == PREDICT_FRAMES) return false;
                    shift(goalItems, it, f - it->startFrame);
                    
                    // Continue and see if we created another supply block later
                    continue;
                }

                // There was no active supply provider to move
                // Let's see when we have the resources to queue a new one

                // Find the frame when we have enough minerals to produce the pylon
                int desiredStartFrame = std::max(0, f - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                int mineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() + it->type.mineralPrice();
                int mineralFrame;
                for (mineralFrame = PREDICT_FRAMES - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                {
                    if (minerals[mineralFrame] < mineralsNeeded) break;
                    if (mineralFrame == it->startFrame) mineralsNeeded -= it->type.mineralPrice();
                }
                mineralFrame++;

                if (mineralFrame == PREDICT_FRAMES) return false;

                // Detect the case where this is the first run through and we have a supply block at the time we want to build this item
                // In some cases it will be advantageous to delay the previous item to resolve the supply block earlier
                if (f == it->startFrame && mineralFrame != desiredStartFrame && it != goalItems.begin())
                {
                    const ProductionItem * previous = nullptr;
                    for (auto previousIt = goalItems.begin(); previousIt != goalItems.end() && previousIt != it; previousIt++)
                    {
                        previous = &*previousIt;
                    }

                    if (previous && previous->startFrame < mineralFrame)
                    {
                        // First figure out when we can produce the pylon given we didn't produce the previous item
                        int pylonMineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() - previous->type.mineralPrice();
                        int pylonMineralFrame;
                        for (pylonMineralFrame = mineralFrame; pylonMineralFrame >= desiredStartFrame; pylonMineralFrame--)
                        {
                            if (minerals[pylonMineralFrame] < pylonMineralsNeeded) break;
                            if (pylonMineralFrame == previous->startFrame) mineralsNeeded += previous->type.mineralPrice();
                        }
                        pylonMineralFrame++;

                        // Now check if we can produce the current item earlier
                        if (std::max(mineralFrame - previous->startFrame, pylonMineralFrame - desiredStartFrame) < (mineralFrame - desiredStartFrame))
                        {
                            // Find the previous item in the committed items
                            for (auto committedIt = committedItems.begin(); committedIt != committedItems.end(); committedIt++)
                            {
                                if (committedIt->type != previous->type ||
                                    committedIt->startFrame != previous->startFrame ||
                                    committedIt->existingProducer != previous->existingProducer ||
                                    committedIt->queuedProducer != previous->queuedProducer)
                                {
                                    continue;
                                }

                                // We found it, perform the adjustments
                                spendResource(minerals, previous->startFrame, -previous->type.mineralPrice());
                                spendResource(minerals, mineralFrame, previous->type.mineralPrice());

                                // We reinsert it so it gets sorted correctly
                                committedItems.emplace(*committedIt, mineralFrame - previous->startFrame);
                                committedItems.erase(committedIt);

                                // Shift all goal items appropriately
                                shift(goalItems, it, mineralFrame - previous->startFrame);
                                f = it->startFrame;

                                // Pylon is now being produced at the new frame we found
                                mineralFrame = pylonMineralFrame;
                                break;
                            }
                        }
                    }
                }

                // Queue the supply provider
                committedItems.emplace(BWAPI::UnitTypes::Protoss_Pylon, mineralFrame);
                spendResource(supply, mineralFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon), -BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());
                spendResource(minerals, mineralFrame, BWAPI::UnitTypes::Protoss_Pylon.mineralPrice());

                // If the block is resolved, continue the main loop
                if (mineralFrame == desiredStartFrame) continue;

                // Otherwise shift the item to the next frame where we have supply for it
                for (; f < PREDICT_FRAMES; f++)
                {
                    if (supply[f] >= it->type.supplyRequired()) break;
                }
                if (f == PREDICT_FRAMES) return false;
                shift(goalItems, it, f - it->startFrame);
            }

            return true;
        }

        void resolveResourceBlocksAndCommit(ProductionItemSet & goalItems, ProductionItemSet & committedItems)
        {
            bool cancelAll = false;
            for (auto it = goalItems.begin(); it != goalItems.end(); )
            {
                // Remove items beyond frame PREDICT_FRAMES or if we couldn't afford an earlier item
                if (cancelAll || it->startFrame >= PREDICT_FRAMES)
                {
                    // If the item has a queued producer attached, erase it as well; it is no longer useful since it can't produce any units
                    if (it->queuedProducer && !it->queuedProducer->queuedBuilding)
                    {
                        spendResource(minerals, it->queuedProducer->startFrame, -it->queuedProducer->type.mineralPrice());
                        spendResource(gas, it->queuedProducer->startFrame, -it->queuedProducer->type.gasPrice());
                        goalItems.erase(*it->queuedProducer);
                    }

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
                spendResource(minerals, it->startFrame, it->type.mineralPrice());
                if (it->type.gasPrice() > 0) spendResource(gas, it->startFrame, it->type.gasPrice());
                if (it->type.supplyRequired() > 0) spendResource(supply, it->startFrame, it->type.supplyRequired());

                // If the item is a worker, assume it will go on minerals when complete
                if (it->type.isWorker())
                    updateResourceCollection(minerals, it->completionFrame, 1, MINERALS_PER_WORKER_FRAME);

                // If the item is a refinery, move three workers to it when it finishes
                // TODO: allow more fine-grained selection of gas workers
                if (it->type == BWAPI::Broodwar->self()->getRace().getRefinery())
                {
                    updateResourceCollection(minerals, it->completionFrame, -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, it->completionFrame, 3, GAS_PER_WORKER_FRAME);
                }

                // If the item provides supply, add it
                if (it->type.supplyProvided() > 0)
                    spendResource(supply, it->startFrame, -it->type.supplyProvided());

                // We commit refineries and supply providers immediately
                // This helps to simplify the reordering logic in shiftForGas and shiftForSupply
                if (it->type == BWAPI::Broodwar->self()->getRace().getRefinery() || it->type.supplyProvided() > 0)
                {
                    committedItems.insert(*it);
                    it = goalItems.erase(it);
                }
                else
                    it++;
            }

            // Commit remaining goal items
            committedItems.insert(goalItems.begin(), goalItems.end());
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
            if (goal->isFulfilled()) continue;

            int countToProduce = goal->countToProduce();
            int producerCount = 0;

            auto type = goal->unitType();
            auto producerType = type.whatBuilds().first;
            ProductionItemSet goalItems;

            // Step 1: Ensure we have all buildings we need to build this type

            // If this item isn't a building, ensure we have something that can produce it
            if (!type.isBuilding()) addBuildingIfIncomplete(goalItems, producerType);

            // Add other missing prerequisites for the type
            addMissingPrerequisites(goalItems, type);

            // Shift the buildings so the frames start at 0
            normalizeStartFrame(goalItems);

            // Resolve duplicates, keeping the earliest one of each type
            resolveDuplicates(goalItems, committedItems);

            // Reserve positions for all buildings
            reserveBuildPositions(goalItems, committedItems);

            // Record the frame when prerequisites are available
            int prerequisitesAvailable = goalItems.empty() ? 0 : goalItems.rbegin()->completionFrame;

            // Step 2: Insert production from all existing or planned producers

            // Insert production for planned producers
            for (auto & item : goalItems)
                if (item.type == producerType && !item.queuedBuilding)
                {
                    insertProduction(goalItems, type, std::max(prerequisitesAvailable, item.completionFrame), &item);
                    producerCount++;
                }

            // Insert production for completed producers
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->exists() || !unit->isCompleted()) continue;
                if (unit->getType() != producerType) continue;

                // The remaining train time may not be completely accurate if the train command is queued, so also check the last command
                int remainingTrainTime = unit->getRemainingTrainTime();
                if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Train &&
                    (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame()) <= BWAPI::Broodwar->getLatencyFrames())
                {
                    remainingTrainTime = UnitUtil::BuildTime(unit->getLastCommand().getUnitType());
                }

                insertProduction(goalItems, type, std::max(prerequisitesAvailable, remainingTrainTime), unit);
                producerCount++;
            }

            // Insert production for pending producers
            for (auto building : Builder::pendingBuildingsOfType(producerType))
            {
                insertProduction(goalItems, type, std::max(prerequisitesAvailable, building->expectedFramesUntilCompletion()));
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
            int producerLimit = goal->producerLimit();
            while ((producerLimit == -1 || producerCount < producerLimit) && !goalItems.empty())
            {
                goalItems.clear();

                // This just repeats most of the above steps but only considering adding one producer
                int startFrame = std::max(0, prerequisitesAvailable - UnitUtil::BuildTime(producerType));
                auto producer = goalItems.emplace(producerType, startFrame);
                producerCount++;

                reserveBuildPositions(goalItems, committedItems);
                insertProduction(goalItems, type, producer->completionFrame, &*producer);
                resolveResourceBlocksAndCommit(goalItems, committedItems);
            }
        }

        write(committedItems);

        // Issue build commands
        for (auto & item : committedItems)
        {
            // Produce units desired at frame 0
            if (item.existingProducer && item.startFrame == 0)
            {
                item.existingProducer->train(item.type);
            }

            // Pylons may not already have a build location
            if (item.type == BWAPI::UnitTypes::Protoss_Pylon && !item.buildLocation.tile.isValid())
            {
                // TODO: Allow items to specify their neighbourhood
                auto neighbourhood = BuildingPlacement::Neighbourhood::MainBase;

                choosePylonBuildLocation(neighbourhood, item);
            }

            // Queue buildings when we expect the builder will arrive at the desired start frame or later
            if (item.type.isBuilding() && !item.queuedBuilding &&
                (item.startFrame - item.buildLocation.builderFrames) <= 0)
            {
                // TODO: Should be resolved earlier
                if (!item.buildLocation.tile.isValid()) continue;

                int arrivalFrame;
                auto builder = Builder::getBuilderUnit(item.buildLocation.tile, item.type, &arrivalFrame);
                if (builder && arrivalFrame >= item.startFrame)
                {
                    Builder::build(item.type, item.buildLocation.tile, builder);

                    // TODO: This needs to be handled better
                    if (item.type == BWAPI::Broodwar->self()->getRace().getRefinery())
                    {
                        Workers::setDesiredGasWorkers(Workers::gasWorkers() + 3);
                    }
                }
            }
        }
    }
}
