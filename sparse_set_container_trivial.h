// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

template<typename T>
class SparseSetContainer_Trivial
{
public:
	using IndexType = uint32_t;

	/// SparseSetContainer key
	/// used to uniquely identify an entry
	/// not expected to be serialized
	class Key
	{
	private:
		Key(IndexType sparseIdx)
			: mSparseIdx(sparseIdx)
		{}

		IndexType mSparseIdx;

		friend SparseSetContainer_Trivial<T>;
	};

public:
	SparseSetContainer_Trivial() = default;
	// copying a sparse set container doesn't make much sense
	SparseSetContainer_Trivial(const SparseSetContainer_Trivial&) = delete;
	SparseSetContainer_Trivial& operator=(const SparseSetContainer_Trivial&) = delete;
	SparseSetContainer_Trivial(SparseSetContainer_Trivial&& other) noexcept
		: mDenseValueArray(std::move(other.mDenseValueArray))
		, mDenseKeyArray(std::move(other.mDenseKeyArray))
		, mSparseArray(std::move(other.mSparseArray))
		, mNextFreeSparseEntry(other.mNextFreeSparseEntry)
	{
		other.mNextFreeSparseEntry = InvaliSparseIndex;
	}

	SparseSetContainer_Trivial& operator=(SparseSetContainer_Trivial&& other) noexcept
	{
		mDenseValueArray = std::move(other.mDenseValueArray);
		mDenseKeyArray = std::move(other.mDenseKeyArray);
		mSparseArray = std::move(other.mSparseArray);
		mNextFreeSparseEntry = other.mNextFreeSparseEntry;
		other.mNextFreeSparseEntry = InvaliSparseIndex;

		return *this;
	}

	void reserve(size_t newCapacity)
	{
		mDenseValueArray.reserve(newCapacity);
		mDenseKeyArray.reserve(newCapacity);
		mSparseArray.reserve(newCapacity);
	}

	std::span<T> getValues() noexcept { return mDenseValueArray; }
	std::span<const T> getValues() const noexcept { return mDenseValueArray; }
	std::vector<T> consumeValues() noexcept
	{
		mDenseKeyArray.clear();
		mSparseArray.clear();
		mNextFreeSparseEntry = InvaliSparseIndex;
		return std::move(mDenseValueArray);
	}

	void clear()
	{
		mDenseValueArray.clear();
		mDenseKeyArray.clear();
		mSparseArray.clear();
		mNextFreeSparseEntry = InvaliSparseIndex;
	}

	T* get(Key key) noexcept
	{
		if (key.mSparseIdx >= mSparseArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		const IndexType denseIdx = mSparseArray[key.mSparseIdx].denseIndexOrNextFree;

		if (denseIdx >= mDenseValueArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		if (mDenseKeyArray[denseIdx].mSparseIdx != key.mSparseIdx) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		return &mDenseValueArray[denseIdx];
	}

	const T* get(Key key) const noexcept
	{
		if (key.mSparseIdx >= mSparseArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		const IndexType denseIdx = mSparseArray[key.mSparseIdx].denseIndexOrNextFree;

		if (denseIdx >= mDenseValueArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		if (mDenseKeyArray[denseIdx].mSparseIdx != key.mSparseIdx) [[unlikely]]
		{
			// this should never happen in a valid app
			return nullptr;
		}

		return &mDenseValueArray[denseIdx];
	}

	template<typename U>
	[[nodiscard]] Key pushBack(U&& value)
	{
		const IndexType newDenseIdx = static_cast<IndexType>(mDenseValueArray.size());

		if (newDenseIdx > MaxValidIndex) [[unlikely]]
		{
			return Key{ InvaliSparseIndex };
		}

		IndexType sparseIndex;
		if (mNextFreeSparseEntry != InvaliSparseIndex)
		{
			sparseIndex = mNextFreeSparseEntry;
			mNextFreeSparseEntry = mSparseArray[sparseIndex].denseIndexOrNextFree;
			mSparseArray[sparseIndex].denseIndexOrNextFree = newDenseIdx;
		}
		else
		{
			sparseIndex = mSparseArray.size();
			mSparseArray.emplace_back(newDenseIdx);
		}

		mDenseValueArray.emplace_back(std::forward<U>(value));
		mDenseKeyArray.push_back(Key(sparseIndex));
		return Key{ sparseIndex };
	}

	void removeSwap(Key key) noexcept
	{
		if (key.mSparseIdx >= mSparseArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return;
		}

		const IndexType denseIdx = mSparseArray[key.mSparseIdx].denseIndexOrNextFree;

		if (denseIdx >= mDenseValueArray.size()) [[unlikely]]
		{
			// this should never happen in a valid app
			return;
		}

		if (mDenseKeyArray[denseIdx].mSparseIdx != key.mSparseIdx) [[unlikely]]
		{
			// this should never happen in a valid app
			return;
		}

		if (denseIdx + 1 != mDenseValueArray.size())
		{
			std::swap(mSparseArray[key.mSparseIdx], mSparseArray[mDenseKeyArray[mDenseValueArray.size() - 1].mSparseIdx]);
			std::swap(mDenseValueArray[denseIdx], mDenseValueArray[mDenseValueArray.size() - 1]);
			std::swap(mDenseKeyArray[denseIdx], mDenseKeyArray[mDenseKeyArray.size() - 1]);
		}
		mDenseValueArray.pop_back();
		mDenseKeyArray.pop_back();

		mSparseArray[key.mSparseIdx].denseIndexOrNextFree = mNextFreeSparseEntry;
		mNextFreeSparseEntry = key.mSparseIdx;
	}

private:
	struct SparseEntry
	{
		// if alive: dense_index; if free: next_free_sparse_index
		uint32_t denseIndexOrNextFree;
	};

private:
	constexpr static IndexType MaxValidIndex = std::numeric_limits<IndexType>::max() - 1;
	constexpr static IndexType InvaliSparseIndex = std::numeric_limits<IndexType>::max();

	std::vector<T> mDenseValueArray;
	std::vector<Key> mDenseKeyArray;
	std::vector<SparseEntry> mSparseArray;
	IndexType mNextFreeSparseEntry = InvaliSparseIndex;
};
