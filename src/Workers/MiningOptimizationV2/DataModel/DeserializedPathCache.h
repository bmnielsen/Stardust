#pragma once

#include "MapData.h"

#define CAPACITY 500

#define LOG_CACHE_OPERATIONS false

/*
 * Caches the most recently used deserialized paths, so if paths come up often, we don't burn a lot of CPU time deserializing them.
 * We use a multimap for the index to allow the cache to store multiple copies of the same path if two workers need to use the same one at the same
 * time.
 */
namespace MiningOptimization
{
    typedef std::pair<TilePosition, PositionAndVelocity> DeserializedPathCacheKey;

    template <typename ObservationType>
    class DeserializedPathCache
    {
    public:
        struct Item
        {
            DeserializedPathCacheKey key;
            std::unique_ptr<Path<ObservationType>> path;
        };

        DeserializedPathCache(
            const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType> > > &pathData)
            : pathData(pathData)
        {
        }

        std::optional<Item> get(const DeserializedPathCacheKey &key)
        {
            // Check if this key is already in the cache
            auto cacheIt = index.find(key);
            if (cacheIt != index.end())
            {
                // We got a cache hit: remove the path from the cache and return it
                auto result = Item{key, std::move(cacheIt->second->second)};
                cache.erase(cacheIt->second);
                index.erase(cacheIt);

#if LOG_CACHE_OPERATIONS
                Log::Get() << "Cache hit for " << key.first << " / " << key.second;
#endif
                return result;
            }

            // Try to find the serialized path
            auto patchDataIt = pathData.find(key.first);
            if (patchDataIt == pathData.end()) return std::nullopt;
            auto serializedPathIt = patchDataIt->second.find(key.second);
            if (serializedPathIt == patchDataIt->second.end()) return std::nullopt;

#if LOG_CACHE_OPERATIONS
            Log::Get() << "Cache miss for " << key.first << " / " << key.second;
#endif

            // Deserialize the path and return it
            return Item{key, std::make_unique<Path<ObservationType>>(serializedPathIt->second.get())};
        }

        void put(Item &&item)
        {
            // If the cache is at its capacity, remove the least-recently-added item
            if (cache.size() == CAPACITY)
            {
#if LOG_CACHE_OPERATIONS
                Log::Get() << "Cache purge of " << cache.back().first.first << " / " << cache.back().first.second;
#endif

                auto indexIt = index.find(cache.back().first);
                if (indexIt != index.end()) index.erase(indexIt);
                cache.pop_back();
            }

#if LOG_CACHE_OPERATIONS
            Log::Get() << "Cache put of " << item.key.first << " / " << item.key.second;
#endif

            cache.emplace_front(item.key, std::move(item.path));
            index.emplace(item.key, cache.begin());
        }

    private:
        const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>>> &pathData;

        std::list<std::pair<DeserializedPathCacheKey, std::unique_ptr<Path<ObservationType>>>> cache;
        std::multimap<DeserializedPathCacheKey,
            typename std::list<std::pair<DeserializedPathCacheKey, std::unique_ptr<Path<ObservationType>>>>::iterator> index;
    };

    template <typename ObservationType>
    class NullDeserializedPathCache
    {
    public:
        struct Item
        {
            DeserializedPathCacheKey key;
            std::unique_ptr<Path<ObservationType>> path;
        };

        NullDeserializedPathCache(
            const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType> > > &pathData)
            : pathData(pathData)
        {
        }

        std::optional<Item> get(const DeserializedPathCacheKey &key)
        {
            // Try to find the serialized path
            auto patchDataIt = pathData.find(key.first);
            if (patchDataIt == pathData.end()) return std::nullopt;
            auto serializedPathIt = patchDataIt->second.find(key.second);
            if (serializedPathIt == patchDataIt->second.end()) return std::nullopt;

            // Deserialize the path and return it
            return Item{key, std::make_unique<Path<ObservationType>>(serializedPathIt->second.get())};
        }

        void put(Item &&item)
        {
        }

    private:
        const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>>> &pathData;
    };
}
