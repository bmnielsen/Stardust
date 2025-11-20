#pragma once

#include "Path.h"

namespace MiningOptimization
{
    template <typename ObservationType>
    class SerializedPath
    {
    public:
        // The position, including velocity and heading
        PositionAndVelocity pos;

        Path<ObservationType> get() const;

        static SerializedPath<ObservationType> create(const Path<ObservationType> &path);

        template <typename S>
        void serialize(S& s) {
            s.object(pos);
            s.container(data, INT_MAX, [&](S &s, uint8_t &v) {
                s.value1b(v);
            });
        }

    private:
        std::vector<uint8_t> data;
    };
}
