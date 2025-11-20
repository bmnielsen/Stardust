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

            auto customVectorSerializer = [&]()
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
                            if constexpr (std::is_same_v<U, PathNode<ObservationType>>)
                            {
                                s.object(k, pathNodeSerializer);
                            }
                            else
                            {
                                s.object(k);
                            }
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
                            if constexpr (std::is_same_v<U, PathNode<ObservationType>>)
                            {
                                s.object(item, pathNodeSerializer);
                            }
                            else
                            {
                                s.object(item);
                            }

                            vec.emplace_back(std::move(item), occurrenceRate);

                            total += occurrenceRate;
                        }
                        vec.shrink_to_fit();
                    };
                }
            }();

            pathNodeSerializer = [&](S &s, PathNode<ObservationType>& value)
            {
                s.object(value.pos);
                s.object(value.arrivalData, customVectorSerializer);
                if (!value.arrivalData.empty()) s.object(value.arrivalDataAfterResend, customVectorSerializer);
                s.object(value.nextPositions, customVectorSerializer);
                if (!value.nextPositions.empty()) s.object(value.nextPositionsAfterResend, customVectorSerializer);
            };

            ser.object(path.nextPositions, customVectorSerializer);
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
