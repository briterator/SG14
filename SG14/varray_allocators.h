#pragma once
#include <memory>
#include "algorithm_ext.h"
template<uint32_t BufferCount>
struct buffer_allocator
{
	template<typename T>
	struct typed
	{
		constexpr typed() noexcept(true) = default;

		typed(typed&& other) = delete;
		typed(const typed& other) = delete;

		void assign(typed&& other, size_t other_size) noexcept(true)
		{
			auto otherdata = other.data();
			stdext::uninitialized_move(otherdata, otherdata + other_size, data());
			stdext::destroy(otherdata, otherdata + other_size);
		}
		void assign(const typed& other, size_t other_size) noexcept(true)
		{
			auto otherdata = other.data();
			std::uninitialized_copy(otherdata, otherdata + other_size, data());
		}

		T* data() const
		{
			return (T*)std::begin(buffer);
		}
		int64_t max_count() const
		{
			return BufferCount;
		}
		int64_t realloc_exact(int64_t size, int64_t desired_size, int64_t capacity)
		{
			assert(desired_size <= BufferCount);
			return BufferCount;
		}
		int64_t realloc(int64_t size, int64_t desired_size, int64_t capacity)
		{
			assert(desired_size <= BufferCount);
			return BufferCount;
		}
		void free(int64_t count, int64_t capacity_)
		{
			stdext::destroy(data(), data() + count);
		}

	private:

		typename std::aligned_storage<sizeof(T), alignof(T)>::type buffer[BufferCount];
	};
};

namespace math
{
	uint64_t ceil_log2(uint64_t a) // ceil(log2(a))
	{
		unsigned long index;
		if (_BitScanReverse64(&index, a))
		{
			return index + ((a & (a - 1)) ? 1 : 0);
		}
		return 0;
	}

	int64_t next_power_of_two(uint64_t a)
	{
		return int64_t(1) << ceil_log2(a);
	}

}
template<size_t Min>
struct grow_default
{
	int64_t operator()(int64_t size, int64_t desired_size)
	{
		return (desired_size == 0) ? 0 : ((desired_size < Min) ? Min : math::next_power_of_two(desired_size));
	}
};
template<class GrowthPolicy = grow_default<32>>
struct heap_allocator
{
	template<typename T>
	struct typed
	{
		T* data_ = nullptr;
		typed() noexcept(true) = default;
		typed(typed&&) = delete;
		typed(const typed&) = delete;
		void assign(const typed& other, size_t othersize) noexcept(true)
		{
			if (othersize > 0)
			{
				data_ = (T*) ::malloc(othersize * sizeof(T));
				std::uninitialized_copy_n(other.data(), othersize, data_);
			}
		}
		void assign(typed&& other, size_t othersize) noexcept(true)
		{
			data_ = other.data_;
			other.data_ = nullptr;
		}

		T* data() const
		{
			return data_;
		}
		int64_t max_count() const
		{
			return std::numeric_limits<int64_t_t>::max();
		}
		int64_t realloc_exact(int64_t size, int64_t desired_size, int64_t capacity)
		{
			if (desired_size == 0)
			{
				this->free(size, capacity);
			}
			else if (std::is_trivial<T>::value)
			{
				data_ = (T*)::realloc(data_, desired_size * sizeof(T));
			}
			else
			{
				auto new_data = (T*) ::malloc(desired_size * sizeof(T));
				stdext::uninitialized_move(data_, data_ + size, new_data);
				this->free(size, capacity);
				data_ = new_data;
			}
			return desired_size;
		}
		int64_t realloc(int64_t size, int64_t desired_size, int64_t capacity)
		{
			GrowthPolicy g;
			desired_size = g(size, desired_size);
			return realloc_exact(size, desired_size, capacity);
		}
		void free(int64_t size, int64_t capacity)
		{
			stdext::destroy(data_, data_ + size);
			::free(data_);
			data_ = nullptr;
		}
	};
};
template<class Alloc1, class Alloc2 = heap_allocator<grow_default<32>>>
struct fallback_allocator
{

	template<class T>
	struct typed
	{
		typename Alloc1::template typed<T> a_;
		typename Alloc2::template typed<T> b_;

		/** Default constructor. */
		typed() noexcept(true) = default;
		typed(const typed& other) = delete;
		typed(typed&& other) = delete;

		void assign(typed&& other, int64_t othersize)
		{
			if (othersize < other.a_.max_count())
			{
				a_.assign(std::move(other.a_), othersize);
			}
			else
			{
				b_.assign(std::move(other.b_), othersize);
			}
		}
		void assign(const typed& other, int64_t othersize)
		{
			if (othersize < other.a_.max_count())
			{
				a_.assign(other.a_, othersize);
			}
			else
			{
				b_.assign(other.b_, othersize);
			}
		}

		T* data() const
		{
			auto b_data = b_.data();
			return b_data ? b_data : a_.data();
		}
		int64_t max_count() const
		{
			return std::max(a_.max_count(), b_.max_count());
		}
		template<class Op>
		int64_t do_realloc(int64_t old_size, int64_t desired_size, int64_t capacity, Op op)
		{
			if (desired_size <= a_.max_count())
			{
				//data will fit into a_
				auto result = op(a_, old_size, desired_size, capacity);
				auto bdata = b_.data();
				if (bdata)
				{
					//needs to be moved from b_
					std::move(bdata, bdata + old_size, a_.data());
					b_.free(old_size, capacity);
				}
				return result;
			}
			else
			{
				//data must fit into b_
				auto oldbdata = b_.data();
				auto result = op(b_, old_size, desired_size, capacity);
				if (!oldbdata)
				{
					//data needs to be moved from a_ to b_
					std::move(a_.data(), a_.data() + old_size, b_.data());
					a_.free(old_size, capacity);
				}

				return result;
			}
		}
		int64_t realloc_exact(int64_t old_size, int64_t desired_size, int64_t capacity)
		{
			return do_realloc(old_size, desired_size, capacity, [](auto& a, auto b, auto c, auto d) { return a.realloc_exact(b, c, d); });
		}
		int64_t realloc(int64_t old_size, int64_t desired_size, int64_t capacity)
		{
			return do_realloc(old_size, desired_size, capacity, [](auto& a, auto b, auto c, auto d) { return a.realloc(b, c, d); });
		}
		void free(int64_t size, int64_t capacity)
		{
			auto bdata = b_.data();
			if (bdata)
			{
				b_.free(size, capacity);
			}
			else
			{
				a_.free(size, capacity);
			}
		}
	};
};

template<size_t N>
using bufheap_allocator = fallback_allocator< buffer_allocator<N> >;

#if 0
struct example_poly_alloc
{
	template<class T>
	struct typed
	{
		std::memory_resource* pmr;
		T* ptr;
		example_poly_alloc()
			:pmr(nullptr)
		{}
		example_poly_alloc(std::experimental::memory_resource* resource)
			:pmr(resource), ptr(nullptr)
		{
		};

		void assign(typed&& other, int64_t othersize)
		{
			pmr = other.pmr;
			other.pmr = nullptr;
		}
		void assign(const typed& other, int64_t othersize)
		{
			ptr = (T*) pmr->allocate(othersize, std::alignment_of<T>::value);
			std::uninitialized_copy(other.ptr, other.ptr+othersize, ptr);
		}
		int64_t realloc_exact(int64_t old_size, int64_t desired_size, int64_t capacity)
		{
			if (desired_size == 0)
			{
				this->free(old_size, capacity);
			}
			else
			{
				auto new_ptr = pmr->allocate(desired_size, std::alignment_of<T>::value);
				stdext::uninitialized_move(ptr, ptr + old_size, new_pmr);
				this->free(old_size, capacity);
				ptr = new_ptr;
			}
		}
		int64_t realloc(int64_t old_size, int64_t desired_size, int64_t capacity)
		{
			//todo set up a growth policy for example_poly_alloc?
			return realloc_exact(old_size, desired_size, capacity);
		}
		void free(int64_t size, capacity)
		{
			stdext::destroy(pmr, pmr + size);
			pmr->deallocate(pmr, capacity, std::alignment_of<T>::value);
		}
		T* data() const
		{
			return ptr;
		}
		int64_t max_count() const
		{
			return std::numeric_limits<int64_t>::max();
		}

	};
};
#endif