#pragma once

#include "Common.h"
#include "Resource.h"
#include "MyWorker.h"

namespace WorkerMiningInstrumentation
{
    struct Efficiency
    {
        double singleWorkerRotationTime;
        double singleWorkerMiningPercentage;
        double doubleWorkerRotationTime;
        double doubleWorkerMiningPercentage;
        double collisionRate;

        friend std::ostream & operator << (std::ostream &os, const Efficiency &obj)
        {
            std::ostringstream buffer;
            buffer << std::fixed << std::setprecision(1);

            std::string sep;
            auto add = [&](const std::string &label, double value)
            {
                buffer << sep;
                buffer << label << ": " << value;
                sep = "; ";
            };

            if (obj.singleWorkerMiningPercentage > 0.01)
            {
                add("Single rotation", obj.singleWorkerRotationTime);
                add("Single mining %", obj.singleWorkerMiningPercentage);
            }

            if (obj.doubleWorkerMiningPercentage > 0.01)
            {
                add("Double rotation", obj.doubleWorkerRotationTime);
                add("Double mining %", obj.doubleWorkerMiningPercentage);
            }

            if (obj.collisionRate > 0.01)
            {
                add("Collision %", (obj.collisionRate * 100.0));
            }

            os << buffer.str();
            return os;
        }
    };

    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride = nullptr);

    void update();

    void writeInstrumentation();

    void trackCollisionObservation(const Resource &patch, bool collision);

    std::map<Resource, Efficiency> getEfficiencyByPatch(int fromFrame = -1, int toFrame = -1);

    Efficiency getEfficiency(int fromFrame = -1, int toFrame = -1);

    int getFiftiethMineralFrame();

    std::vector<int> &getThousandMineralFrames();

    void addRotationTimesToResourceObservations();
}
