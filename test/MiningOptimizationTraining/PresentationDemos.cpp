#include "BWTest.h"

#include "Modules/InstrumentedDoNothingModule.h"
#include "Modules/DoNothingModule.h"

namespace
{
    class Command
    {
    public:
        virtual ~Command() = default;

        Command(int frame, int worker) : frame(frame), worker(worker) {}
        int frame;
        int worker;
        virtual void issue(BWAPI::Unit unit) = 0;
    };

    class GatherCommand : public Command
    {
    public:
        GatherCommand(int frame, int worker, BWAPI::TilePosition target) : Command(frame, worker), target(target) {}

        BWAPI::TilePosition target;

        void issue(BWAPI::Unit unit) override
        {
            for (auto patch : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (!patch->getType().isMineralField()) continue;
                if (patch->getTilePosition() == target)
                {
                    unit->gather(patch);
                    CherryVis::log(unit->getID()) << "Issued gather command to patch @ " << BWAPI::WalkPosition(patch->getPosition());
                    return;
                }
            }
            Log::Get() << "ERROR: Could not find patch @ " << target;
        }
    };

    class ReturnCommand : public Command
    {
    public:
        ReturnCommand(int frame, int worker) : Command(frame, worker) {}

        void issue(BWAPI::Unit unit) override
        {
            unit->returnCargo();
            CherryVis::log(unit->getID()) << "Issued return cargo command";
        }
    };

    class MoveCommand : public Command
    {
    public:
        MoveCommand(int frame, int worker, BWAPI::Position target) : Command(frame, worker), target(target) {}

        BWAPI::Position target;

        void issue(BWAPI::Unit unit) override
        {
            unit->move(target);
            CherryVis::log(unit->getID()) << "Issued move command to " << BWAPI::WalkPosition(target);
        }
    };

    class PresentationDemoModule : public InstrumentedDoNothingModule
        {
        public:
            explicit PresentationDemoModule(std::vector<std::unique_ptr<Command>> &&commands)
                : InstrumentedDoNothingModule(false)
                , commands(std::move(commands))
                {}

            void onStart() override
            {
                InstrumentedDoNothingModule::onStart();

                // Gather workers
                for (const auto &unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        workers.push_back(unit);
                    }
                }

                // Sort the workers by location so we get stable behaviour across runs
                std::sort(workers.begin(), workers.end(), [](const BWAPI::Unit &a, const BWAPI::Unit &b)
                {
                    return a->getPosition() < b->getPosition();
                });
            }

            void onFrame() override
            {
                onFrameStart();

                for (const auto &command : commands)
                {
                    if (command->frame == BWAPI::Broodwar->getFrameCount())
                    {
                        command->issue(workers[command->worker]);
                    }
                }

                onFrameEnd();
            }

        protected:
            std::vector<std::unique_ptr<Command>> commands;
            std::vector<BWAPI::Unit> workers;
    };

    void run(std::vector<std::unique_ptr<Command>> &&commands, bool patchLockingTest = false)
    {
        BWTest test;
        test.map = Maps::GetOne("Vermeer");
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = [&]()
        {
            return new PresentationDemoModule(std::move(commands));
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = true;
        test.frameLimit = 500;
        if (patchLockingTest)
        {
            test.onStartMine = []()
            {
                for (auto mineralPatch : BWAPI::Broodwar->getStaticNeutralUnits())
                {
                    if (mineralPatch->getType().isMineralField() && mineralPatch->getTilePosition() != BWAPI::TilePosition(5, 12))
                    {
                        BWAPI::Broodwar->killUnit(mineralPatch);
                    }
                }
            };
        }
        test.run();
    }
}

TEST(PresentationDemos, NormalCollection)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, OrderProcessTimerReset)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(125, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, ResetOnWaitForMinerals)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(107, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, ResetOnMiningMinerals)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(106, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, ResetOnDelivery)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(125, 0, BWAPI::TilePosition(2,7)));
    commands.push_back(std::make_unique<GatherCommand>(173, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, GatherResend)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(2,7)));
    commands.push_back(std::make_unique<GatherCommand>(20, 0, BWAPI::TilePosition(2,7)));
    run(std::move(commands));
}

TEST(PresentationDemos, ResetDuringGatherRepathing)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<MoveCommand>(1, 0, BWAPI::Position(1000, 1000)));
    commands.push_back(std::make_unique<GatherCommand>(4, 0, BWAPI::TilePosition(2,8)));
    run(std::move(commands));
}

TEST(PresentationDemos, ReturnResend)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(2,7)));
    commands.push_back(std::make_unique<ReturnCommand>(152, 0));
    run(std::move(commands));
}

TEST(PresentationDemos, ReturnPreserveSpeed)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(120, 0, BWAPI::TilePosition(3,12)));
    commands.push_back(std::make_unique<GatherCommand>(155, 0, BWAPI::TilePosition(3,12)));
    commands.push_back(std::make_unique<ReturnCommand>(286, 0));
    run(std::move(commands));
}

TEST(PresentationDemos, CollisionWithPatch)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(2,8)));
    run(std::move(commands));
}

TEST(PresentationDemos, CollisionWithDepot)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(5,12)));
    run(std::move(commands));
}

TEST(PresentationDemos, PatchSwitch)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(5,12)));
    commands.push_back(std::make_unique<GatherCommand>(40, 1, BWAPI::TilePosition(5,12)));
    run(std::move(commands));
}

TEST(PresentationDemos, PatchLock)
{
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<GatherCommand>(1, 0, BWAPI::TilePosition(5,12)));
    commands.push_back(std::make_unique<GatherCommand>(40, 1, BWAPI::TilePosition(5,12)));
    run(std::move(commands), true);
}
