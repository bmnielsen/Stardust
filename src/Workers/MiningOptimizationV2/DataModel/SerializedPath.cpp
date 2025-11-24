#include "SerializedPath.h"
#include "GatherArrivalData.h"
#include "ReturnArrivalData.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/vector.h>

namespace MiningOptimization
{
    namespace
    {
        uint8_t zero = 0;

        template <bool serializing, typename S, typename ObservationType>
        void serializePath(S &ser, Path<ObservationType> &path)
        {
            std::function<void(S&, PathNode<ObservationType>&)> pathNodeSerializer;
            auto resendArrivalDataSerializer = [](S &s, ObservationType &arrivalData)
            {
                s.value1b(arrivalData.arrivalDelay);
                s.value1b(arrivalData.packed);
            };
            auto finalNodeArrivalDataSerializer = [](S &s, ObservationType &arrivalData)
            {
                arrivalData.arrivalDelay = 1;
                s.value1b(arrivalData.packed);
            };

            auto customVectorSerializer = [&](const auto &itemSerializer)
            {
                if constexpr (serializing)
                {
                    return [&]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                    {
                        // If the vector is empty, just write a zero
                        if (vec.empty())
                        {
                            s.value1b(zero);
                            return;
                        }

                        // Write the occurrences before the nodes
                        for (auto &[k, v] : vec)
                        {
                            s.value1b(v);
                            s.object(k, itemSerializer);
                        }
                    };
                }
                else
                {
                    return [&]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                    {
                        uint8_t total = 0;
                        while (total < 255)
                        {
                            uint8_t occurrenceRate;
                            s.value1b(occurrenceRate);

                            // First item will be zero for an empty vector
                            if (occurrenceRate == 0) return;

                            U item;
                            s.object(item, itemSerializer);
                            vec.emplace_back(std::move(item), occurrenceRate);

                            total += occurrenceRate;
                        }
                        vec.shrink_to_fit();
                    };
                }
            };

            pathNodeSerializer = [&](S &s, PathNode<ObservationType>& value)
            {
                s.object(value.pos);

                // Final nodes (where there is arrival without the maximum number of available resends) have a subset of arrival data,
                // where the arrival delay is not needed (since it is always 0 there)
                // Final resend nodes have the arrival data after resend and no other data.
                // All in between nodes have no arrival data, only next positions if resends are stable or unavailable, and next positions after
                // resend if both are available.
                // As intermediate nodes are the most common, we always write the nextPositions vector. If it is empty, we are at the final node
                // and only need to write the subset of arrival data.
                // If there are next positions, we also write the next positions after resend.
                // If there are no next positions after resend, we write the arrival data after resend.
                s.object(value.nextPositions, customVectorSerializer(pathNodeSerializer));
                if (value.nextPositions.empty())
                {
                    s.object(value.arrivalDataWhenFinalNode, customVectorSerializer(finalNodeArrivalDataSerializer));
                }
                else
                {
                    s.object(value.nextPositionsAfterResend, customVectorSerializer(pathNodeSerializer));
                    if (value.nextPositionsAfterResend.empty())
                    {
                        s.object(value.arrivalDataAfterResend, customVectorSerializer(resendArrivalDataSerializer));
                    }
                }
            };

            ser.object(path.nextPositions, customVectorSerializer(pathNodeSerializer));
        }
    }

    template <typename ObservationType>
    Path<ObservationType> SerializedPath<ObservationType>::get() const
    {
        Path<ObservationType> result;
        result.pos = pos;
        bitsery::Deserializer<bitsery::InputBufferAdapter<std::vector<uint8_t>>> ser{data.begin(), data.size()};
        serializePath<false>(ser, result);
        return std::move(result);
    }

    template <typename ObservationType>
    SerializedPath<ObservationType> SerializedPath<ObservationType>::create(const Path<ObservationType> &path)
    {
        SerializedPath<ObservationType> result;
        result.pos = path.pos;
        bitsery::Serializer<bitsery::OutputBufferAdapter<std::vector<uint8_t>>> ser{result.data};
        serializePath<true>(ser, const_cast<Path<ObservationType>&>(path));
        return std::move(result);
    }

    template class SerializedPath<GatherArrivalData>;
    template class SerializedPath<ReturnArrivalData>;
}
