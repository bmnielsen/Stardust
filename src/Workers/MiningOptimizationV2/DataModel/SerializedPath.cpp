#include "SerializedPath.h"
#include "MapData.h"

#include "Units.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/vector.h>

namespace MiningOptimization
{
    namespace
    {
        uint8_t zero = 0;
        uint8_t one = 1;

        template <bool serializing, typename S, typename ObservationType>
        void serializePath(S &ser, Path<ObservationType> &path)
        {
            std::function<void(S&, PathNode<ObservationType>&)> pathNodeSerializer;
            auto resendArrivalDataSerializer = [](S &s, ObservationType &arrivalData)
            {
                if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                {
                    s.value1b(arrivalData.packed);
                    s.value1b(arrivalData.tenDistanceAndResendAlwaysArrivesIndex);
                }
                else
                {
                    s.value1b(arrivalData.packedArrivalDelayAndCollision);
                    s.value1b(arrivalData.packedExitSpeedAndFacingDepot);
                }
#if USE_NEXT_PATH_LENGTHS
                s.value1b(arrivalData.nextPathLengthDelta);
#endif
            };
            auto finalNodeArrivalDataSerializer = [](S &s, ObservationType &arrivalData)
            {
                if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                {
                    s.value1b(arrivalData.packed);
                    s.value1b(arrivalData.resendAlwaysArrivesDelta);
                }
                else
                {
                    s.value1b(arrivalData.packedArrivalDelayAndCollision);
                    s.value1b(arrivalData.packedExitSpeedAndFacingDepot);
                }
#if USE_NEXT_PATH_LENGTHS
                // TODO: If this gets enabled again, pack the data like we used to so we don't have to include the arrival delay here
                s.value1b(arrivalData.nextPathLengthDelta);
#endif
            };

            auto customVectorSerializer = [&](const auto &itemSerializer, bool *extraPackedBool = nullptr)
            {
                if constexpr (serializing)
                {
                    return [&itemSerializer, extraPackedBool]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                    {
                        // If the vector is empty, write zero or one depending on whether we want to pack a bool
                        if (vec.empty())
                        {
                            if (extraPackedBool && *extraPackedBool)
                            {
                                s.value1b(one);
                            }
                            else
                            {
                                s.value1b(zero);
                            }
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
                    return [&itemSerializer, extraPackedBool]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                    {
                        uint8_t total = 0;
                        while (total < OCCURRENCE_SCALE)
                        {
                            uint8_t occurrenceRate;
                            s.value1b(occurrenceRate);

                            // A zero always indicates an empty vector
                            if (occurrenceRate == 0) return;

                            // If there is a packed bool, a one in the first item indicates an empty vector with the packed bool set
                            if (extraPackedBool && total == 0 && occurrenceRate == 1)
                            {
                                *extraPackedBool = true;
                                return;
                            }

                            U item;
                            s.object(item, itemSerializer);
                            vec.emplace_back(std::move(item), occurrenceRate);

                            total += occurrenceRate;
                        }
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
                // The "isStableResendNode" boolean only has to be serialized when nextPositionsAfterResend and arrivalDataAfterResend are both
                // empty, since stable nodes don't need those data. To serialize this efficiently, we borrow a bit in the first
                // arrivalDataAfterResend occurrence rate, setting it to 1 to indicate an empty vector with this bool set.
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
                        s.object(value.arrivalDataAfterResend, customVectorSerializer(resendArrivalDataSerializer, &value.isStableResendNode));
                    }
                }
            };

            ser.object(path.nextPositions, customVectorSerializer(pathNodeSerializer));
        }
    }

    template <typename ObservationType>
    std::optional<CannonPlacement> SerializedPath<ObservationType>::activeCannonPlacement() const
    {
        for (auto &[cannonPlacement, _] : dataByCannonPlacement)
        {
            if (cannonPlacement.cannonCount > 0 && !Units::myBuildingAt(cannonPlacement.tile)) continue;

            return cannonPlacement;
        }

        return std::nullopt;
    }

    template <typename ObservationType>
    Path<ObservationType> SerializedPath<ObservationType>::get(std::optional<CannonPlacement> cannonPlacement) const
    {
        if (!cannonPlacement) cannonPlacement = activeCannonPlacement();
        if (!cannonPlacement) return {pos};

        for (auto &[placement, data] : dataByCannonPlacement)
        {
            if (placement != *cannonPlacement) continue;

            Path<ObservationType> result;
            result.pos = pos;
            result.cannonPlacement = placement;
            bitsery::Deserializer<bitsery::InputBufferAdapter<std::vector<uint8_t>>> ser{data.begin(), data.size()};
            serializePath<false>(ser, result);
            return std::move(result);
        }

        // Shouldn't get here
        return {pos};
    }

    template <typename ObservationType>
    SerializedPath<ObservationType> SerializedPath<ObservationType>::create(
            const std::map<CannonPlacement, Path<ObservationType>> &cannonPlacementToPath)
    {
        SerializedPath<ObservationType> result;
        std::vector<std::pair<CannonPlacement, std::vector<uint8_t>>> dataByCannonPlacement;
        for (const auto &[cannonPlacement, path] : cannonPlacementToPath)
        {
            result.pos = path.pos;

            std::vector<uint8_t> serialized;
            bitsery::Serializer<bitsery::OutputBufferAdapter<std::vector<uint8_t>>> ser{serialized};
            serializePath<true>(ser, const_cast<Path<ObservationType>&>(path));

            dataByCannonPlacement.emplace_back(cannonPlacement, std::move(serialized));
        }

        std::sort(dataByCannonPlacement.begin(), dataByCannonPlacement.end(), [](const std::pair<CannonPlacement, std::vector<uint8_t>> &a,
                                                                                 const std::pair<CannonPlacement, std::vector<uint8_t>> &b)
        {
            return a.first.cannonCount > b.first.cannonCount;
        });

        // Remove any data vectors that are equivalent to a lower number of cannons
        for (auto it = dataByCannonPlacement.begin(); it != dataByCannonPlacement.end(); )
        {
            bool areEqual = false;
            for (auto nextIt = it + 1; nextIt != dataByCannonPlacement.end(); nextIt++)
            {
                if (nextIt->first.cannonCount == it->first.cannonCount) continue;

                if (it->second == nextIt->second)
                {
                    areEqual = true;
                    break;
                }
            }

            if (areEqual)
            {
                it = dataByCannonPlacement.erase(it);
            }
            else
            {
                it++;
            }
        }

        result.dataByCannonPlacement = std::move(dataByCannonPlacement);
        return std::move(result);
    }

    template class SerializedPath<GatherArrivalData>;
    template class SerializedPath<ReturnArrivalData>;
}
