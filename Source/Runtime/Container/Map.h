#pragma once

#include <cassert>
#include <unordered_map>
#include <utility>
#include <initializer_list>
#include "Core/Types.h"
#include <functional>


template <typename T, typename V, typename Hash = std::hash<T>>
class TMap
{
public:
	TMap() = default;
	TMap(std::initializer_list<std::pair<const T, V>> initList) : mMap(initList) {}

	~TMap() = default;

	typename std::unordered_map<T, V, Hash>::iterator begin();
	typename std::unordered_map<T, V, Hash>::iterator end();

	typename std::unordered_map<T, V, Hash>::const_iterator begin() const;
	typename std::unordered_map<T, V, Hash>::const_iterator end() const;

	void Add(const T& key, const V& Value);
	int32 Remove(const T& key);
	
	uint32 Num() const;
	void Reset();
	void Empty(int32 ExpectedNumElements = 0);
	V* FindOrNull(const T& key);
	const V* FindOrNull(const T& key) const;

	V* Find(const T& key);
	const V* Find(const T& key) const;

	bool Contains(const T& key) const;
	bool IsEmpty() const;
	void Reserve(int32 Capacity);

	V& operator[](const T& key);
	const V& operator[](const T& key) const;

private:
	std::unordered_map<T, V, Hash> mMap;
};

template <typename T, typename V, typename Hash>
inline typename std::unordered_map<T, V, Hash>::iterator TMap<T, V, Hash>::begin()
{
	return mMap.begin();
}

template <typename T, typename V, typename Hash>
inline typename std::unordered_map<T, V, Hash>::iterator TMap<T, V, Hash>::end()
{
	return mMap.end();
}

template <typename T, typename V, typename Hash>
inline typename std::unordered_map<T, V, Hash>::const_iterator TMap<T, V, Hash>::begin() const
{
	return mMap.cbegin();
}

template <typename T, typename V, typename Hash>
inline typename std::unordered_map<T, V, Hash>::const_iterator TMap<T, V, Hash>::end() const
{
	return mMap.cend();
}

template <typename T, typename V, typename Hash>
inline void TMap<T, V, Hash>::Add(const T& key, const V& value)
{
	mMap[key] = value;
}

template <typename T, typename V, typename Hash>
inline int32 TMap<T, V, Hash>::Remove(const T& key)
{
	return static_cast<int32>(mMap.erase(key));
}

template <typename T, typename V, typename Hash>
inline uint32 TMap<T, V, Hash>::Num() const
{
	return static_cast<uint32>(mMap.size());
}

template <typename T, typename V, typename Hash>
inline void TMap<T, V, Hash>::Reset()
{
	mMap.clear();
}

template <typename T, typename V, typename Hash>
inline void TMap<T, V, Hash>::Empty(int32 capacity)
{
	mMap.clear();
	mMap.reserve(static_cast<size_t>(capacity));
}

template <typename T, typename V, typename Hash>
inline V* TMap<T, V, Hash>::FindOrNull(const T& key)
{
	auto iter = mMap.find(key);

	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V, typename Hash>
inline const V* TMap<T, V, Hash>::FindOrNull(const T& key) const
{
	auto iter = mMap.find(key);

	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V, typename Hash>
inline V* TMap<T, V, Hash>::Find(const T& key)
{
	auto iter = mMap.find(key);

	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V, typename Hash>
inline const V* TMap<T, V, Hash>::Find(const T& key) const
{
	auto iter = mMap.find(key);

	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V, typename Hash>
inline bool TMap<T, V, Hash>::Contains(const T& key) const
{
	return mMap.find(key) != mMap.end();
}

template <typename T, typename V, typename Hash>
inline bool TMap<T, V, Hash>::IsEmpty() const
{
	return mMap.empty();
}

template <typename T, typename V, typename Hash>
inline void TMap<T, V, Hash>::Reserve(int32 capacity)
{
	mMap.reserve(static_cast<size_t>(capacity));
}

template <typename T, typename V, typename Hash>
inline V& TMap<T, V, Hash>::operator[](const T& key)
{
	return mMap[key];
}

template <typename T, typename V, typename Hash>
inline const V& TMap<T, V, Hash>::operator[](const T& key) const
{
	return mMap.at(key);
}