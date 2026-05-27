#pragma once

#include "Path.h"
#include "CannonPlacement.h"

#include <optional>

namespace MiningOptimization
{
    template <typename ObservationType>
    class SerializedPath
    {
    public:
        // The position, including velocity and heading
        PositionAndVelocity pos;

        [[nodiscard]] std::optional<CannonPlacement> activeCannonPlacement() const;

        [[nodiscard]] Path<ObservationType> get(std::optional<CannonPlacement> cannonPlacement = std::nullopt) const;

        static SerializedPath<ObservationType> create(const std::map<CannonPlacement, Path<ObservationType>> &cannonPlacementToPath);

        template <typename S>
        void serialize(S& s) {
            s.object(pos);
            s.container(dataByCannonPlacement, INT_MAX, [&](S &s, std::pair<CannonPlacement, std::vector<uint8_t>> &v) {
                s.object(v.first);
                s.container(v.second, INT_MAX, [&](S &s, uint8_t &v) {
                    s.value1b(v);
                });
            });
        }

    private:
        std::vector<std::pair<CannonPlacement, std::vector<uint8_t>>> dataByCannonPlacement;
    };
}
