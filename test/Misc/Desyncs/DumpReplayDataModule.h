#pragma once

#include "BWAPI.h"
#include <fstream>

/*
 * Module intended to be attached to a replay.
 *
 * Dumps selected unit data for the two players to a CSV file, one line per frame per unit.
 */
class DumpReplayDataModule : public BWAPI::AIModule
{
public:
    explicit DumpReplayDataModule(std::string bwapiBackendLabel) : bwapiBackendLabel(std::move(bwapiBackendLabel)) {}

    void onStart() override
    {
        filename = BWAPI::Broodwar->mapPathName() + "." + bwapiBackendLabel + ".csv";
        file.open(filename, std::ofstream::trunc);
        file << "Frame;ID;Type;PosX;PosY;vX;vY;Angle;Flying;HP;Shields;GrdCdwn;AirCdwn\n";

        std::cout << "Started replay dump of " << BWAPI::Broodwar->mapPathName() << std::endl;

        BWAPI::Broodwar->setLocalSpeed(0);
    }

    void onFrame() override
    {
        for (int i = 0; i < 2; i++)
        {
            std::vector<BWAPI::Unit> units;

            for (auto unit : BWAPI::Broodwar->getPlayer(i)->getUnits())
            {
                if (unit->exists()) units.push_back(unit);
            }

            std::sort(units.begin(), units.end(), [](BWAPI::Unit a, BWAPI::Unit b)
            {
                if (a->getType() != b->getType())
                {
                    return (int)(a->getType()) < (int)(b->getType());
                }

                if (a->getPosition() != b->getPosition())
                {
                    return a->getPosition() < b->getPosition();
                }

                if ((int)std::round(a->getVelocityX() * 1000.0) != (int)std::round(b->getVelocityX() * 1000.0))
                {
                    return (int)std::round(a->getVelocityX() * 1000.0) < (int)std::round(b->getVelocityX() * 1000.0);
                }

                if ((int)std::round(a->getVelocityY() * 1000.0) != (int)std::round(b->getVelocityY() * 1000.0))
                {
                    return (int)std::round(a->getVelocityY() * 1000.0) < (int)std::round(b->getVelocityY() * 1000.0);
                }

                if ((int)std::round(a->getAngle() * 1000.0) != (int)std::round(b->getAngle() * 1000.0))
                {
                    return (int)std::round(a->getAngle() * 1000.0) < (int)std::round(b->getAngle() * 1000.0);
                }

                return a->getID() < b->getID();
            });

            for (const auto &unit : units)
            {
                file << BWAPI::Broodwar->getFrameCount() << ";"
                     << unit->getID() << ";"
                     << unit->getType() << ";"
                     << unit->getPosition().x << ";"
                     << unit->getPosition().y << ";"
                     << (int)std::round(unit->getVelocityX() * 1000.0) << ";"
                     << (int)std::round(unit->getVelocityY() * 1000.0) << ";"
                     << (int)std::round(unit->getAngle() * 1000.0) << ";"
                     << unit->isBurrowed() << ";"
                     << unit->getHitPoints() << ";"
                     << unit->getShields() << ";"
                     << unit->getGroundWeaponCooldown() << ";"
                     << unit->getAirWeaponCooldown() << "\n";
            }
        }

        if (BWAPI::Broodwar->getFrameCount() > 0 && BWAPI::Broodwar->getFrameCount() % 1000 == 0)
        {
            std::cout << "Processed " << BWAPI::Broodwar->getFrameCount() << " frames" << std::endl;
        }
    }

    void onEnd(bool) override
    {
        file.close();
        std::cout << "Game ended, results written to " << filename << std::endl;
    }

private:
    std::string bwapiBackendLabel;
    std::string filename;
    std::ofstream file;
};
