#pragma once
#include <utility>
#include <memory>
#include <algorithm>
#include "algorithm_ext.h"
//hot stands for hash, open-addressing w/ tombstone
//note: this class is untested and incomplete

struct default_open_addressing_load_algorithm
{
	//how many elements can fit in this many buckets
	size_t occupancy(size_t allocated)
	{
		auto half = allocated >> 1;
		return half + (half >> 2);
	}

	//how many buckets needed to fulfill this many elements
	size_t allocated(size_t occupied)
	{
		if (occupied == 0)
			return 0;
		occupied += occupied;
		auto l = log2(occupied);
		auto c = ceil(l);
		return size_t(1) << int(c);
	}

	template<class T>
	T* select(T* begin, T* end, size_t hash) const
	{
		return begin + (hash & ((end - begin) - 1));
	}

	size_t grow(size_t allocated)
	{
		return std::max<size_t>(32, allocated << 1);
	}
};

template<class T>
struct variable
{
	T value;
	variable()
		:value()
	{}
	variable(const T& value_)
		:value(value_)
	{}
	variable(T&& value_)
		:value(std::move(value_))
	{}

	const T& operator()() const
	{
		return value;
	}
};

template<class T>
struct span
{
	T* first;
	T* last;

	T* begin() { return first;  }
	T* end() { return last;  }
};

template<
	class T,					//contained type
	class Tomb,					//tombstone generator function
	class Equal = std::equal_to<void>,//element comparator
	class Alloc = std::allocator<T>,
	class Hash = std::hash<T>,		
	class Load = default_open_addressing_load_algorithm//load algorithm
>
class hot_set
{
	T* mbegin;
	T* mend;
	size_t mcapacity;
	size_t moccupied;
	Hash hash;
	Load load_alg;
	Equal eq;
	Tomb tomb_gen;
	Alloc allocator;

	void init(size_t size)
	{
		if (size > 0)
		{
			//assert(powerOfTwo(size));
			mbegin = allocator.allocate(size);
			mend = mbegin + size;
			std::uninitialized_fill(mbegin, mend, tomb_gen());
			moccupied = 0;
			mcapacity = load_alg.occupancy(size);
		}
	}

	void rehash(size_t newsize)
	{
		auto oldbegin = mbegin;
		auto oldend = mend;
		mcapacity = load_alg.occupancy(newsize);

		auto tomb = tombstone();
		auto b = allocator.allocate(newsize);
		auto e = b + newsize;
		std::uninitialized_fill(b, e, tomb);
		auto it = oldbegin;
		auto equal = eq;
		while (it != oldend)
		{
			auto& value = *it;
			if (!equal(tomb, value))
			{
				*find_internal(b, e, value).first = std::move(value);
			}
			++it;
		}
		mbegin = b;
		mend = e;
		allocator.deallocate(oldbegin, oldend-oldbegin);		
	}

	void partial_rehash(T* first, T* pos, T* last)
	{
		auto tomb = tomb_gen();
		auto current = pos;
		for (int i = 0; i < 2; ++i)
		{
			while (current != last)
			{
				auto& value = *current;
				if (eq(value, tomb))
				{
					return;
				}
				else
				{
					auto temp = std::move(value);
					value = tomb;
					*find_internal(mbegin, mend, temp).first = std::move(temp);
				}
				++current;
			};
			current = first;
			last = pos;
		}
	}
	void remove_internal(T* first_, T* element_, T* last_)
	{
		--moccupied;
		*element_ = tomb_gen();
		partial_rehash(first_, element_, last_);
	}
	auto find_internal(T* first_, T* last_, const T& in_) const
	{
		auto start = load_alg.select(first_, last_, hash(in_));
		auto equal = eq;
		auto tomb = tomb_gen();
		auto current = start;
		for (int i = 0; i < 2; ++i)
		{
			while (current != last_)
			{
				auto& value = *current;
				if (equal(tomb, value))
				{
					return std::make_pair(current, false);
				}
				else if (equal(in_, value))
				{
					return std::make_pair(current, true);
				}
				++current;
			};
			last_ = start;
			current = first_;
		};

		return std::make_pair(last_, false);
	}
public:
	struct iterator : std::iterator< std::forward_iterator_tag, T>
	{
		const hot_set& set;
		T* current;
		iterator(const iterator&) = default;
		iterator(iterator&&) = default;
		iterator(T* current_, const hot_set& set_)
			:current(current_), set(set_)
		{
			advance();
		}

		const T& operator*() const
		{
			return *current;
		}
		void advance()
		{
			current = std::find_if(current, set.mend, [&](T& elem) { return !set.is_invalid(elem); });
		}
		iterator operator++(int)
		{
			iterator r(*this);
			++r;
			return r;
		}
		iterator& operator++()
		{
			++current;
			advance();
			return *this;
		}
		bool operator!=(iterator other) const
		{
			return current != other.current;
		}
		bool operator==(iterator other) const
		{
			return current == other.current;
		}
		const T* base() const
		{
			return current;
		}
	};

	hot_set()
		: mbegin()
		, mend()
		, mcapacity()
		, moccupied()
	{}

	hot_set(const hot_set& in)
		: allocator(in.allocator)
		, tomb_gen(in.tomb_gen)
		, mcapacity(in.mcapacity)
		, moccupied(in.moccupied)
		, hash(in.hash)
		, load_alg(in.load_alg)
		, eq(in.eq)
	{
		auto size = in.reserved();
		mbegin = allocator.allocate(size);
		mend = mbegin + size;
		std::uninitialized_copy(in.mbegin, in.mend, mbegin);
	}

	hot_set(hot_set&& in)
		: mbegin(in.mbegin)
		, mend(in.mend)
		, mcapacity(in.mcapacity)
		, moccupied(in.moccupied)
		, Alloc(std::move(in))
		, hash(std::move(in.hash))
		, cap(std::move(in.cap))
		, eq(std::move(in.eq))
	{
		in.mcapacity = 0;
		in.moccupied = 0;
		in.mbegin = nullptr;
		in.mend = nullptr;
	}

	hot_set(size_t capacity_, Tomb tombstone_ = Tomb(), Hash hash_ = Hash(), Equal equal_ = Equal(), Load load_ = Load(), Alloc alloc_ = Alloc())
		: hash(std::move(hash_))
		, load_alg(std::move(load_))
		, eq(std::move(equal_))
		, allocator(std::move(alloc_))
		, tomb_gen(std::move(tombstone_))
		, moccupied(0)
		, mcapacity(0)
		, mbegin(nullptr)
		, mend(nullptr)
	{
		init(load_alg.allocated(capacity_));
	}

	hot_set& operator=(const hot_set& other_)
	{
		~hot_set();
		return *new(this) hot_set(other_);
	}
	hot_set& operator=(hot_set&& other_)
	{
		~hot_set();
		return *new(this) hot_set(std::move(other_));
	}
	bool is_invalid(const T& value_) const
	{
		return eq(tombstone(), value_);
	}

	//number of elements allocated by the set
	size_t reserved() const
	{
		return mend - mbegin;
	}

	//number of elements the set may contain before reallocating
	size_t capacity() const
	{
		return mcapacity;
	}

	//number of elements in the set
	size_t size() const
	{
		return moccupied;
	}

	span<T> raw_view()
	{
		return{ mbegin, mend };
	}

	void shrink()
	{
		auto target_size = load_alg.allocated(mcapacity);
		if (target_size != (mend - mbegin) )
		{
			rehash(target_size);
		}
	}

	//Inserts an element into the set
	//If size() == capacity(), invalidates any iterators
	template<class U>
	auto insert(U&& value_)
	{
		if (mcapacity != moccupied) {}else
		{
			rehash(load_alg.grow(mend - mbegin));
		}
		return stable_insert(std::forward<U>(value_));
	}

	//Inserts an element into the set
	//precondition: size < capacity
	//invalidates no iterators
	template<class U>
	auto stable_insert(U&& value_)
	{
		auto result = find_internal(mbegin, mend, value_);
		*result.first = std::forward<U>(value_);
		moccupied += uint32_t(result.second == false);
		return result;
	}
	//removes element. invalidates all iterators.
	void erase(const T* element_)
	{
		remove_internal(mbegin, element_, mend);
	}
	//removes element == value. invalidates all iterators.
	bool erase(const T& value_)
	{
		auto b = mbegin;
		auto e = mend;
		auto found = find_internal(b, e, value_);
		if (found.second)
		{
			remove_internal(b, found.first, e);
			return true;
		}
		return false;
	}
	decltype(auto) tombstone() const
	{
		return tomb_gen();
	}

	bool empty() const
	{
		return moccupied == 0;
	}

	//invalidates all iterators
	bool clear()
	{
		std::fill(begin, end, tombstone());
		moccupied = 0;
	}

	auto find(const T& value_) const
	{
		return find_internal(mbegin, mend, value_);
	}

	bool contains(const T& value_) const
	{
		return find_internal(mbegin, mend, value_).second;
	}

	auto begin() const
	{
		return iterator(mbegin, *this);
	}

	auto end() const
	{
		return iterator(mend, *this);
	}

	~hot_set()
	{
		stdext::destroy(mbegin, mend);
		allocator.deallocate(mbegin, mend-mbegin);
	}
};
template<class T> using hov_set = hot_set< T, variable<T> >;
template<class T, T tombstone> using hoc_set = hot_set< T, std::integral_constant<T, tombstone> >;
#if 0
template<class K, class V>
struct hot_pair
{
	K key;
	V value;

	operator const K&() const
	{
		return key;
	}
	bool operator==(const hot_pair<K, V>& other) const
	{
		return other.key == key && other.value == value;
	}
};
template<class K, class V>
struct hot_pair_by_key
{
	bool operator()(const hot_pair<K,V>& a, const hot_pair<K,V>& b) const
	{
		return a.key == b.key;
	}
};


namespace std
{
	template<class K, class V>
	struct hash<hot_pair<K, V>>
	{
		size_t operator()(const hot_pair<K, V>& p) const
		{
			std::hash<K> h;
			return h(p.key);
		}
	};
}


//Map implementation, unique keys
//The user provides a key which shall never be inserted

template<
	class K, class V,
	class Tomb,	
	class Eq = std::equal_to<void>,
	class Alloc = std::allocator<hot_pair<K,V>>,
	class Hash = std::hash<K>,
	class Cap = default_open_addressing_load_algorithm
>
class hot_map
{
	hot_set<hot_pair<K, V>, Tomb, Eq, Alloc, Hash, Cap> data;
public:
	hot_map() = default;
	hot_map(hot_map&&) = default;
	hot_map(const hot_map&) = default;
	hot_map(size_t init_capacity, K tombstone_key, V tombstone_value,  Hash h = Hash(), Cap c = Cap(), Alloc alloc = Alloc())
		: Super(init_capacity, hot_pair<K,V>(std::move(tombstone_key), std::move(tombstone_value)),  h, c, alloc)
	{}

	template<class KK, class VV>
	auto insert(KK&& key, VV&& value)
	{
		return data.insert(hot_pair<K, V>{std::forward<KK>(key), std::forward<VV>(value)});
	}
};

template<class K, class V> using hov_map = hot_map<K, V, variable<K> , hot_pair_by_key<K,V>>;
template<class K, K tombstone, class V> using hoc_map = hot_map< K, V, std::integral_constant<K, tombstone>, hot_pair_by_key<K,V>>;


//Map implementation, duplicate keys allowed
//the user provides a key-value pair which shall never be inserted
template<
	class K, class V,
	class Tomb,
	class Eq = std::equal_to<void>,
	class Alloc = std::allocator<hot_pair<K, V>>,
	class Hash = std::hash<K>,
	class Cap = default_open_addressing_load_algorithm
>
class hot_multimap : public hot_map<K, V, Tomb, Eq, Alloc, Hash, Cap>
{
	typedef hot_map<K, V, Tomb, Eq, Alloc, Hash, Cap> Super;
public:
	hot_multimap() = default;
	hot_multimap(hot_multimap&&) = default;
	hot_multimap(const hot_multimap&) = default;

	hot_multimap(size_t init_capacity, K tombstone_key, V tombstone_value=V(), Hash h = Hash(), Cap c = Cap(), Alloc alloc = Alloc())
		: Super(init_capacity, std::move(tombstone_key), std::move(tombstone_value), h, c, alloc)
	{}

	template<class KK, class VV>
	auto find(KK&& key, VV&& val) const
	{
		return Super::find(hot_pair<K, V>{std::forward<KK>(key), std::forward<VV>(val)});
	}
};


template<class K, class V> using hod_multimap = hot_multimap<K, V, variable<hot_pair<K, V>> >;
#endif