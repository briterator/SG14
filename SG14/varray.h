#pragma once

#include "span.h"
#include "varray_allocators.h"

//variable-length array similar to std vector
// provides no exception guarantees
template<typename T, typename Allocator = heap_allocator<grow_default<32>> >
class varray
{
	int64_t count_;
	int64_t capacity_;
	typename Allocator::template typed<T> allocator_;
public:
	using ElementT = T;
	constexpr varray() noexcept(true)
		: count_(0), capacity_(0), allocator_()
	{}
	template<class... Args>
	varray(std::allocator_arg_t, Args&&... args)
		:count_(0), capacity_(0), allocator_(std::forward<Args>(args))
	{

	}
	varray(std::initializer_list<T> items)
		: count_(items.size())
	{
		capacity_ = allocator_.realloc(0, count_, 0);
		copy({ items.begin(), items.end() });
	}
	varray(span<T> c) noexcept(true)
		:count_(c.size())
	{
		capacity_ = allocator_.realloc(0, count_, 0);
		std::uninitialized_copy(c.begin(), c.end(), begin());
	}

	varray(const varray& other) noexcept(true)
		:count_(other.count_), capacity_(other.count_)
	{
		allocator_.assign(other.allocator_, other.count_);
	}

	varray(varray&& other) noexcept(true)
		: count_(other.count_)
		, capacity_(other.capacity_)
	{
		allocator_.assign(std::move(other.allocator_), count_);
		other.count_ = 0;
		other.capacity_ = 0;
	}

	varray& operator=(varray&& other) noexcept(true)
	{
		this->~varray();
		new(this) varray(std::move(other));
		return *this;
	}

	varray& operator=(const varray& Other) noexcept(true)
	{
		copy(Other);
		return *this;
	}

	varray& operator=(span<const T> Other) noexcept(true)
	{
		copy(Other);
		return *this;
	}

	T& push_front(const T& Item)
	{
		return insert(Item, 0);
	}
	T& push_front(T&& Item)
	{
		return insert(std::move(Item), 0);
	}
	T& push_back(const T& Item)
	{
		grow_capacity(count_ + 1);
		auto e = end();
		new(e) T(Item);
		++count_;
		return *e;
	}
	T& push_back(T&& Item)
	{
		grow_capacity(count_ + 1);
		auto e = end();
		new(e) T(std::move(Item));
		++count_;
		return *e;
	}


	const T* begin() const noexcept(true)
	{
		return (T*)allocator_.data();
	}
	T* begin() noexcept(true)
	{
		return (T*)allocator_.data();
	}

	const T* end() const noexcept(true)
	{
		return begin() + count_;
	}
	T* end() noexcept(true)
	{
		return begin() + count_;
	}

	constexpr auto slack() const noexcept(true)
	{
		return capacity_ - count_;
	}

	constexpr auto capacity() const noexcept(true)
	{
		return capacity_;
	}

	constexpr auto size() const noexcept(true)
	{
		return count_;
	}


	T& operator[](int64_t i)
	{
		assert(i >= 0 && (i<count_));
		return begin()[i];
	}
	const T& operator[](int64_t i) const
	{
		assert(i >= 0 && (i<count_));
		return begin()[i];
	}
	T pop_front()
	{
		auto b = begin();
		auto Result = std::move(*b);
		erase(b);
		return Result;
	}
	T pop_back()
	{
		auto pos = end() - 1;
		auto Result = std::move(*pos);
		erase_from_end(1);
		return Result;
	}
	T& front()
	{
		return *(begin());
	}
	const T& front() const
	{
		return *(begin());
	}

	T& back()
	{
		return *(end() - 1);
	}
	const T& back() const
	{
		return *(end() - 1);
	}

	void shrink_to_fit()
	{
		if (capacity_ != count_)
		{
			capacity_ = allocator_.realloc_exact(count_, count_, capacity_);
		}
	}

	bool operator==(span<const T> OtherArray) const
	{
		return std::equals(view(), OtherArray);
	}
	bool operator!=(span<const T> OtherArray) const
	{
		return !(*this == OtherArray);
	}

	T& insert(const T& item, int64_t index)
	{
		push_back(item);
		auto b = begin();
		std::rotate(b + index, b + count_ - 1, b + count_);
		return *(b + index);
	}
	T& insert(T&& Item, int64_t index)
	{
		push_back(std::move(Item));
		auto b = begin();
		std::rotate(b + index, b + count_ - 1, b + count_);
		return *(b + index);
	}

	template<class... Args >
	T& emplace_back(Args&&... args)
	{
		auto old_count = count_;
		grow_capacity(old_count + 1);
		count_ = old_count + 1;
		return *new(begin() + old_count) T(std::forward<Args>(args)...);
	}
	void clear()
	{
		auto Begin = begin();
		stdext::destroy(Begin, Begin + count_);
		count_ = 0;
	}
	void clear(int64_t slack)
	{
		clear();
		if (capacity_ != slack)
		{
			capacity_ = allocator_.realloc_exact(0, slack, capacity_);
		}
	}

	void operator+=(span<const T> source)
	{
		grow_capacity(count_ + source.size());

		auto mstart = begin() + count_;
		auto sstart = source.begin();
		std::uninitialized_copy_n(sstart, source.size(), mstart);
		count_ += source.size();

	}

	void operator+=(varray&& source)
	{
		grow_capacity(count_ + source.count_);
		stdext::uninitialized_move_n(source.begin(), source.count_, end());
		count_ += source.count_;
	}

	void grow_capacity(int64_t new_capacity)
	{
		if (new_capacity > capacity_)
		{
			capacity_ = allocator_.realloc(count_, new_capacity, capacity_);
		}
	}
	void grow_capacity_exact(int64_t new_capacity)
	{
		if (new_capacity > capacity_)
		{
			capacity_ = allocator_.realloc_exact(count_, new_capacity, capacity_);
		}
	}

	void erase(T* at)
	{
		auto e = end();
		std::move(at + 1, e, at);
		stdext::destroy_at(e - 1);
		count_--;
	}
	int64_t erase_from_end(int64_t num)
	{
		stdext::destroy_n(end() - num, num);
		count_ -= num;
		return num;
	}
	int64_t erase(const T* first, const T* last)
	{
		auto e = end();
		std::move(last, e, first);
		return erase_from_end(last - first);
	}
	void unstable_erase(const T* at)
	{
		auto b = begin();
		auto last = b + count_ - 1;
		if (at != last)
		{
			*at = std::move(*last);
		}
		stdext::destroy_at(last);
		count_--;
	}
	int64_t unstable_erase(const T* first, const T* last)
	{
		auto e = end();
		assert(last >= first && first >= begin() && last <= e);
		std::move(e - count, e, first);
		return erase_from_end(last - first);
	}

	operator span<T>()
	{
		auto b = begin();
		return span<T>(b, b + count_);
	}
	operator span<const T>() const
	{
		return view();
	}

	span<const T> view() const
	{
		auto b = begin();
		return span<const T>(b, b + count_);
	}

	~varray() noexcept(true)
	{
		allocator_.free(count_);
	}

protected:

	void copy(span<const T> other)
	{
		clear(other.size());
		*this += other;
	}


};
