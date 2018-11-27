#include "Producer.h"

#include <array>

#include "Strategist.h"
#include "Units.h"
#include "Builder.h"
#include "BuildingPlacement.h"
#include "Workers.h"

/*
Overall TODOs:
- Not handling buildings queued in builder
- Track powered build locations
*/

namespace Producer
{
    namespace
    {
        // How many frames in the future to consider when predicting future production needs
        const int PREDICT_FRAMES = 2500;

        const double MINERALS_PER_WORKER_FRAME = 0.0456;
        const double GAS_PER_WORKER_FRAME = 0.071;

        int availableGeysers;
        std::array<int, PREDICT_FRAMES> minerals;
        std::array<int, PREDICT_FRAMES> gas;
        std::array<int, PREDICT_FRAMES> supply;

        // Helper type to store what we need to produce when
        struct ProductionItem
        {
            struct cmp
            {
                bool operator()(const ProductionItem & a, const ProductionItem & b) const
                {
                    return a.startFrame < b.startFrame || (!(b.startFrame < a.startFrame) && a.completionFrame < b.completionFrame);
                }

                bool operator()(const ProductionItem * const & a, const ProductionItem * const & b) const
                {
                    return a->startFrame < b->startFrame || (!(b->startFrame < a->startFrame) && a->completionFrame < b->completionFrame);
                }
            };

            BWAPI::UnitType        type;
            mutable int            startFrame;
            mutable int            completionFrame;
            bool                   queued;
            bool                   started;
            const ProductionItem * queuedProducer;
            BWAPI::Unit            existingProducer;

            // Constructor for units
            ProductionItem(BWAPI::UnitType type, int startFrame, int completionFrame, const ProductionItem * queuedProducer = nullptr, BWAPI::Unit existingProducer = nullptr)
                : type(type)
                , startFrame(startFrame)
                , completionFrame(completionFrame)
                , queued(false)
                , started(false)
                , queuedProducer(queuedProducer)
                , existingProducer(existingProducer)
            {}

            // Constructor for buildings
            ProductionItem(BWAPI::UnitType type, int startFrame, int completionFrame, bool queued, bool started)
                : type(type)
                , startFrame(startFrame)
                , completionFrame(completionFrame)
                , queued(queued)
                , started(started)
                , queuedProducer(nullptr)
                , existingProducer(nullptr)
            {}

        };

        typedef std::multiset<ProductionItem, ProductionItem::cmp> ProductionItemSet;
        typedef std::multiset<const ProductionItem *, ProductionItem::cmp> ProductionItemPtrSet;

        void write(ProductionItemSet & items)
        {
            std::ostringstream debug;
            for (auto & item : items)
            {
                if (item.startFrame >= PREDICT_FRAMES) continue;
                debug << "\n" << item.startFrame << ": " << item.type << " (" << item.completionFrame << ")" << " Minerals: " << minerals[item.startFrame] << ", Gas: " << gas[item.startFrame] << ", Supply: " << supply[item.startFrame];
            }
            Log::Debug() << debug.str();
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

        void initializeResources()
        {
            // Initialize the number of geysers available to be built on
            availableGeysers = Workers::availableGeysers();

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

            // Update according to units being built
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->isCompleted()) continue;

                // New workers are assumed to go to minerals
                if (unit->getType().isWorker())
                {
                    updateResourceCollection(minerals, unit->getRemainingBuildTime(), 1, MINERALS_PER_WORKER_FRAME);
                }

                // New refineries are assumed to move three workers to gas
                // TODO: Do this more dynamically
                if (unit->getType() == BWAPI::Broodwar->self()->getRace().getRefinery())
                {
                    updateResourceCollection(minerals, unit->getRemainingBuildTime(), -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, unit->getRemainingBuildTime(), 3, GAS_PER_WORKER_FRAME);
                }

                // Supply providers
                if (unit->getType().supplyProvided() > 0)
                {
                    spendResource(supply, unit->getRemainingBuildTime(), -unit->getType().supplyProvided());
                }
            }
        }

        // Need a forward reference since the next two methods do mutual recursion
        void addMissingPrerequisites(ProductionItemSet & items, BWAPI::UnitType unitType, int startFrame = 0);

        void addBuildingIfIncomplete(ProductionItemSet & items, BWAPI::UnitType unitType, int baseFrame = 0)
        {
            if (Units::countCompleted(unitType) > 0) return;

            // Initial assumption is that the building is not yet started
            int frame = -unitType.buildTime();
            bool queued = false;
            bool started = false;

            // Look for a pending building
            auto pendingBuildings = Builder::pendingBuildingsOfType(unitType);
            for (auto pendingBuilding : pendingBuildings)
            {
                queued = true;
                started = started || pendingBuilding->constructionStarted();
                frame = std::max(frame, -pendingBuilding->expectedFramesUntilCompletion());
            }

            items.emplace(unitType, baseFrame + frame, baseFrame, queued, started);

            addMissingPrerequisites(items, unitType, baseFrame + frame);
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

        void removeDuplicates(ProductionItemSet & items)
        {
            std::set<BWAPI::UnitType> seen;
            for (auto it = items.begin(); it != items.end(); )
            {
                if (seen.find(it->type) != seen.end())
                    it = items.erase(it);
                else
                {
                    seen.insert(it->type);
                    it++;
                }
            }
        }

        void insertProduction(ProductionItemSet & items, BWAPI::UnitType unitType, int frame, const ProductionItem * queuedProducer = nullptr, BWAPI::Unit existingProducer = nullptr)
        {
            bool first = true;
            while (frame < PREDICT_FRAMES)
            {
                items.emplace(unitType, frame, frame + unitType.buildTime(), first ? queuedProducer : nullptr, existingProducer);
                frame += unitType.buildTime();
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
            bool first = true;
            int offset = 0;
            for (auto & item : items)
            {
                if (first)
                {
                    if (item.startFrame >= 0) break;
                    offset = -item.startFrame;
                    first = false;
                }

                item.startFrame += offset;
                item.completionFrame += offset;
            }
        }

        void shift(ProductionItemSet & items, ProductionItemSet::iterator it, int delta)
        {
            if (delta < 1) return;

            int startFrame = it->startFrame;

            for (; it != items.end(); it++)
            {
                it->startFrame += delta;
                it->completionFrame += delta;

                // If this item has a queued producer attached, we should shift it too
                // TODO: Should we extend this to also apply to prerequisite buildings? Would require a list/set and recursion
                if (it->queuedProducer && !it->queuedProducer->queued && !it->queuedProducer->started && it->queuedProducer->startFrame < startFrame)
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
                if (refineryIt->type != refineryType ||
                    refineryIt->started || refineryIt->queued)
                {
                    refineryIt++;
                    continue;
                }
                
                // What frame do we want to move this refinery back to?
                int desiredStartFrame = std::max(0, it->startFrame - gasFramesNeeded - refineryType.buildTime());

                // Find the actual frame we can move it to
                // We check two things:
                // - We have enough minerals to start the refinery at its new start frame
                // - We have enough minerals after the refinery completes and workers are reassigned
                int actualStartFrame = desiredStartFrame;
                for (int f = desiredStartFrame; f < refineryIt->startFrame && f < PREDICT_FRAMES - refineryType.buildTime(); f++)
                {
                    // Start frame
                    if (minerals[f] < refineryType.mineralPrice())
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }

                    // Completion frame
                    int completionFrame = f + refineryType.buildTime();
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
                    int completionFrame = actualStartFrame + refineryType.buildTime();

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
                    updateResourceCollection(minerals, actualStartFrame + refineryType.buildTime(), -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, actualStartFrame + refineryType.buildTime(), 3, GAS_PER_WORKER_FRAME);

                    // We reinsert it so it gets sorted correctly
                    committedItems.emplace(refineryType, actualStartFrame, actualStartFrame + refineryType.buildTime());
                    refineryIt = committedItems.erase(refineryIt);
                }
                else
                    refineryIt++;
            }

            // If we've resolved the gas block, return
            if (gasFramesNeeded == 0) return true;

            // Add a refinery if possible
            if (availableGeysers > 0)
            {
                // Ideal timing makes enough gas available to build the current item
                int desiredStartFrame = std::max(0, it->startFrame - gasFramesNeeded - refineryType.buildTime());

                // Find the actual frame we can start it at
                // We check two things:
                // - We have enough minerals to start the refinery
                // - We have enough minerals after the refinery completes and workers are reassigned
                int actualStartFrame = desiredStartFrame;
                for (int f = desiredStartFrame; f < PREDICT_FRAMES - refineryType.buildTime(); f++)
                {
                    // Start frame
                    if (minerals[f] < refineryType.mineralPrice())
                    {
                        actualStartFrame = f + 1;
                        continue;
                    }

                    // Completion frame
                    int completionFrame = f + refineryType.buildTime();
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
                    updateResourceCollection(minerals, actualStartFrame + refineryType.buildTime(), -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, actualStartFrame + refineryType.buildTime(), 3, GAS_PER_WORKER_FRAME);

                    // Commit
                    committedItems.emplace(refineryType, actualStartFrame, actualStartFrame + refineryType.buildTime());

                    // Mark one less available geyser
                    availableGeysers--;
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
                if (supply[f] >= it->type.supplyRequired()) continue;

                // There is a supply block at this frame, let's try to resolve it
                
                // Look for an existing supply provider active at this time
                const ProductionItem * supplyProvider = nullptr;
                for (auto & potentialSupplyProvider : committedItems)
                {
                    if (potentialSupplyProvider.type.supplyProvided() <= 0) continue;
                    if (potentialSupplyProvider.completionFrame <= f) continue;
                    if (potentialSupplyProvider.started || potentialSupplyProvider.queued) continue;

                    supplyProvider = &potentialSupplyProvider;
                    break;
                }

                // If there is one, attempt to move it earlier
                if (supplyProvider)
                {
                    // Try to shift the supply provider so that it completes at the time of the block
                    int desiredStartFrame = std::max(0, f - supplyProvider->type.buildTime());

                    // Determine how far we can move the supply provider back and still have minerals
                    int mineralsNeeded = supplyProvider->type.mineralPrice() + it->type.mineralPrice();
                    int mineralFrame;
                    for (mineralFrame = supplyProvider->startFrame; mineralFrame >= desiredStartFrame; mineralFrame--)
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
                        spendResource(supply, mineralFrame + supplyProvider->type.buildTime(), -supplyProvider->type.supplyProvided());

                        spendResource(minerals, supplyProvider->startFrame, -supplyProvider->type.mineralPrice());
                        spendResource(minerals, mineralFrame, supplyProvider->type.mineralPrice());

                        // We reinsert it so it gets sorted correctly
                        committedItems.emplace(supplyProvider->type, mineralFrame, mineralFrame + supplyProvider->type.buildTime());
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
                int desiredStartFrame = std::max(0, f - BWAPI::UnitTypes::Protoss_Pylon.buildTime());
                int mineralsNeeded = BWAPI::UnitTypes::Protoss_Pylon.mineralPrice() + it->type.mineralPrice();
                int mineralFrame;
                for (mineralFrame = PREDICT_FRAMES - 1; mineralFrame >= desiredStartFrame; mineralFrame--)
                {
                    if (minerals[mineralFrame] < mineralsNeeded) break;
                    if (mineralFrame == it->startFrame) mineralsNeeded -= it->type.mineralPrice();
                }
                mineralFrame++;

                if (mineralFrame == PREDICT_FRAMES) return false;

                // Queue the supply provider
                committedItems.emplace(BWAPI::UnitTypes::Protoss_Pylon, mineralFrame, mineralFrame + BWAPI::UnitTypes::Protoss_Pylon.buildTime());
                spendResource(supply, mineralFrame + BWAPI::UnitTypes::Protoss_Pylon.buildTime(), -BWAPI::UnitTypes::Protoss_Pylon.supplyProvided());
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
                    // If the item has a queued producer attached, erase it as well, it is no longer useful since it can't produce any units
                    if (it->queuedProducer && !it->queuedProducer->queued && !it->queuedProducer->started)
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

                // If the item is an assimilator, move three workers to it when it finishes
                // TODO: allow more fine-grained selection of gas workers
                if (it->type == BWAPI::UnitTypes::Protoss_Assimilator)
                {
                    updateResourceCollection(minerals, it->completionFrame, -3, MINERALS_PER_WORKER_FRAME);
                    updateResourceCollection(gas, it->completionFrame, 3, GAS_PER_WORKER_FRAME);
                }

                // If the item provides supply, add it
                if (it->type.supplyProvided() > 0)
                    spendResource(supply, it->startFrame, -it->type.supplyProvided());

                // Commit the item
                committedItems.insert(*it);

                //Log::Debug() << "Resolved " << it->type << " @ " << it->startFrame;
                //write(goalItems);
                //write(committedItems);
                it++;
            }
        }
    }

    void update()
    {
        // The overall objective is, for each production goal, to determine what should be produced next and
        // do so whenever this does not delay a higher-priority goal.
        initializeResources();

        ProductionItemSet committedItems;

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

            // Remove duplicates, keeping the earliest one of each type
            removeDuplicates(goalItems);

            // Step 2: Insert continuous production for the next 2000 frames

            // Insert production for completed producers
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->exists() || !unit->isCompleted()) continue;
                if (unit->getType() != producerType) continue;

                insertProduction(goalItems, type, unit->getRemainingTrainTime(), unit);
                producerCount++;
            }

            // Insert production for pending producers
            for (auto building : Builder::pendingBuildingsOfType(producerType))
            {
                insertProduction(goalItems, type, building->expectedFramesUntilCompletion());
                producerCount++;
            }

            // Insert production for planned producers
            for (auto & item : goalItems)
                if (item.type == producerType && !item.started && !item.queued)
                {
                    insertProduction(goalItems, type, std::max(0, item.completionFrame), &item);
                    producerCount++;
                }

            // Remove any excess production
            removeExcessProduction(goalItems, type, countToProduce);

            // Step 3: Normalize the frames to start at 0
            // If we were missing prerequisites, this shifts the production appropriately into the future
            normalizeStartFrame(goalItems);

            // Step 4: Turn our idealized timings into real timings by resolving any resource blocks
            // This will either push or cancel items that we don't have minerals, gas, or supply for,
            // if those problems cannot be resolved
            resolveResourceBlocksAndCommit(goalItems, committedItems);

            //Log::Debug() << "State after committing goal timings:";
            //write(committedItems);

            // Step 6: If the goal wants to add producers, add as many as possible
            // We break when the previous iteration could not commit anything
            int producerLimit = goal->producerLimit();
            while ((producerLimit == -1 || producerCount < producerLimit) && !goalItems.empty())
            {
                goalItems.clear();

                // This just repeats most of the above steps but only considering adding one producer
                auto producer = goalItems.emplace(producerType, 0, producerType.buildTime());
                producerCount++;

                insertProduction(goalItems, type, producerType.buildTime(), &*producer);
                resolveResourceBlocksAndCommit(goalItems, committedItems);

                //if (!goalItems.empty())
                //{
                //    Log::Debug() << "State after adding production:";
                //    write(committedItems);
                //}
            }
        }

        // Issue build commands
        for (auto & item : committedItems)
        {
            // Produce units desired at frame 0
            if (item.existingProducer && item.startFrame == 0)
            {
                item.existingProducer->train(item.type);
            }

            // Queue buildings when we expect the builder will arrive at the desired start frame or later
            //if (item.type.isBuilding())
            //{
            //    auto buildLocation = BuildingPlacement::getBuildingLocation(item.type, BWAPI::Broodwar->self()->getStartLocation());
            //    if (!buildLocation.isValid()) continue;

            //    int arrivalFrame;
            //    auto builder = Builder::getBuilderUnit(buildLocation, item.type, &arrivalFrame);
            //    if (builder && arrivalFrame >= item.startFrame)
            //        Builder::build(item.type, buildLocation, builder);
            //}
        }

    }
}
