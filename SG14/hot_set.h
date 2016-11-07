#pragma once
#include <utility>
#include <memory>
#include <algorithm>
#include "algorithm_ext.h"

namespace sg14
{
	inline size_t first_set_bit(size_t n)
	{
		unsigned long  out=0;
		_BitScanReverse64(&out, n);
		return out;
	}

}
namespace stdext
{
	template<class T, class Alloc>
	T* uninitialized_copy_a(Alloc& a, const T* begin, const T* end, T* dest)
	{
		while (begin != end)
		{
			std::allocator_traits<Alloc>::construct(a, begin, *dest);
			++dest;
			++begin;
		}
		return dest;
	}

	template<class T, class Alloc>
	T* uninitialized_fill_a(Alloc& a, T* begin, T* end, const T& value)
	{
		while (begin != end)
		{
			std::allocator_traits<Alloc>::construct(a, begin, value);
			++begin;
		}
		return begin;
	}

	template<class T, class Alloc>
	void destroy_a(Alloc& a, T* begin, T* end)
	{
		while (begin != end)
		{
			std::allocator_traits<Alloc>::destroy(a, begin);
			++begin;
		}
	}

	//search the range [first, last) for either an element == tombstone or an element == search
	//searches linearly from [probe_start, last) then linearly from [first, probe_start)
	template<class ForwardIterator, class UnaryFunction, class Key>
	auto ring_find_sentinel(ForwardIterator first, ForwardIterator probe_start, ForwardIterator last, UnaryFunction equal, const Key& tombstone, const Key& search)
	{
		for (int i = 0; i < 2; ++i)
		{
			while (probe_start != last)
			{
				auto& current_value = *probe_start;
				auto found = equal(current_value, search);
				if (!equal(current_value, tombstone) && !found)
				{
					++probe_start;
					continue;
				}
				return std::make_pair(probe_start, found);

			}
			last = probe_start;
			probe_start = first;
		}

		return std::make_pair(last, false);
	}
}

struct default_load_policy
{
	//how many elements can fit in this many buckets
	//75% max occupancy
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
		auto l = sg14::first_set_bit(occupied);
		return size_t(1) << l;
	}

	//since this is a power of two algo we can use masking
	template<class T>
	T* select(T* begin, T* end, size_t hash) const
	{
		return begin + (hash & ((end - begin) - 1));
	}

	//doubles in size when growing
	size_t grow(size_t allocated)
	{
		return std::max<size_t>(32, allocated << 1);
	}
};

template<class T>
struct variable
{
	T value_;
	variable() = default;
	variable(variable&&) = default;
	variable(const variable&) = default;
	variable(const T& value)
		:value_(value)
	{}
	variable(T&& value)
		:value_(std::move(value))
	{}
	variable& operator=(const variable&) = default;
	variable& operator=(variable&&) = default;
	variable& operator=(const T& value) { value_ = value; }
	variable& operator=(T&& value) { value_ = std::move(value); }
	const T& operator()() const
	{
		return value_;
	}
};

template<class T>
struct span
{
	T* first_;
	T* last_;

	T* begin() { return first_;  }
	T* end() { return last_;  }
};

//hot stands for hash, open-addressing w/ tombstone
//note: this class is untested and incomplete

template<
	class T,	//contained type
	class Tomb = variable<T>,	//tombstone generator function
	class Equal = std::equal_to<void>,//element comparator
	class Alloc = std::allocator<T>, //allocator
	class Hash = std::hash<T>,	//hasher
	class Load = default_load_policy//controls load factor and related concerns
>
class hot_set
{
	T* begin_;
	size_t allocated_;
	size_t capacity_;
	size_t occupied_;
	Hash hash_;
	Load load_alg_;
	Equal eq_;
	Tomb tomb_gen_;
	Alloc allocator_;

	void init(size_t size)
	{
		if (size > 0)
		{
			begin_ = allocator_.allocate(size);
			allocated_ = size;
			stdext::uninitialized_fill_a(allocator_, begin_, begin_+allocated_, tombstone());
			occupied_ = 0;
			capacity_ = load_alg_.occupancy(size);
		}
	}

	void rehash(size_t newsize)
	{
		auto oldbegin = begin_;
		auto oldend = begin_+allocated_;
		capacity_ = load_alg_.occupancy(newsize);

		auto tomb = tombstone();
		auto b = allocator_.allocate(newsize);
		auto e = b + newsize;
		stdext::uninitialized_fill_a(allocator_, b, e, tomb);
		auto equal = eq_;
		for (auto& elem : span<T>{ oldbegin, oldend } )
		{
			if (!equal(tomb, elem))
			{
				*probe_find(b, e, elem).first = std::move(elem);
			}
		}
		begin_ = b;
		allocated_ = newsize;
		allocator_.deallocate(oldbegin, oldend-oldbegin);		
	}
	void remove_internal(T* first, T* element, T* last)
	{
		--occupied_;
		auto tomb = tombstone();
		*element = tomb;
		
		//rehash elements that may have collided
		probe(first, element, last, [&](T& value)
		{
			auto temp = std::move(value);
			value = tomb;
			*probe_find(first, last, temp).first = std::move(temp);
		});
	}
	auto probe_find(T* const first, T* last, const T& search) const
	{
		auto start = load_alg_.select(first, last, hash_(search));
		return stdext::ring_find_sentinel(first, start, last, eq_, tombstone(), search);
	}

	template<class Func>
	void probe(T* first_, T* start_, T* last_, Func f) const
	{
		auto equal = eq_;
		auto tomb = tombstone();
		auto current = start_;
		for (int i = 0; i < 2; ++i)
		{
			while (current != last_)
			{
				auto& value = *current;
				if (equal(tomb, value))
				{
					return;
				}
				else
				{
					f(value);
				}
				++current;
			}
			last_ = start_;
			current = first_;
		}
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
			auto c = set.tombstone_compare();
			current = std::find_if(current, set.begin_ + set.allocated_, [&c](auto& elem){return !c(elem); });
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
		: begin_()
		, allocated_()
		, capacity_()
		, occupied_()
	{}

	hot_set(const hot_set& in)
		: allocator_(in.allocator_)
		, tomb_gen_(in.tomb_gen_)
		, capacity_(in.capacity_)
		, occupied_(in.occupied_)
		, hash_(in.hash_)
		, load_alg_(in.load_alg_)
		, eq_(in.eq_)
	{
		auto size = in.allocated();
		begin_ = allocator_.allocate(size);
		allocated_ = size;
		stdext::uninitialized_copy_a(allocator_, in.raw_span().begin(), in.raw_span().end(), begin_);
	}

	hot_set(hot_set&& in)
		: begin_(in.begin_)
		, allocated_(in.allocated_)
		, capacity_(in.capacity_)
		, occupied_(in.occupied_)
		, Alloc(std::move(in))
		, hash(std::move(in.hash))
		, load_alg(std::move(in.load_alg))
		, eq(std::move(in.eq))
	{
		in.capacity_ = 0;
		in.occupied_ = 0;
		in.begin_ = nullptr;
		in.allocated_ = 0;
	}

	hot_set(size_t capacity, Tomb tombstone = Tomb(), Hash hash = Hash(), Equal equal = Equal(), Load load = Load(), Alloc alloc = Alloc())
		: hash_(std::move(hash))
		, load_alg_(std::move(load))
		, eq_(std::move(equal))
		, allocator_(std::move(alloc))
		, tomb_gen_(std::move(tombstone))
		, occupied_(0)
		, capacity_(0)
		, begin_(nullptr)
		, allocated_(0)
	{
		init(load_alg_.allocated(capacity));
	}

	hot_set& operator=(const hot_set& other)
	{
		~hot_set();
		return *new(this) hot_set(other);
	}
	hot_set& operator=(hot_set&& other)
	{
		~hot_set();
		return *new(this) hot_set(std::move(other));
	}
	auto tombstone_compare() const
	{
		auto t = tombstone();
		auto equal = eq_;
		return [=](const T& candidate){ return equal(candidate, t); };
	}

	//number of elements allocated by the set
	size_t allocated() const
	{
		return allocated_;
	}

	//number of elements the set may contain before reallocating
	size_t capacity() const
	{
		return capacity_;
	}

	//number of elements in the set
	size_t size() const
	{
		return occupied_;
	}

	span<const T> raw_span() const
	{
		return{ begin_, begin_+allocated_ };
	}
	
	void shrink()
	{
		auto target_size = load_alg.allocated(capacity_);
		if (target_size != allocated_ )
		{
			rehash(target_size);
		}
	}

	//Inserts an element into the set
	//If size() == capacity(), invalidates any iterators
	template<class U>
	auto insert(U&& value)
	{
		if (capacity_ == occupied_)
		{
			rehash(load_alg_.grow(allocated_));
		}
		return stable_insert(std::forward<U>(value));
	}

	//Inserts an element into the set
	//precondition: size < capacity
	//invalidates no iterators
	template<class U>
	auto stable_insert(U&& value)
	{
		auto result = probe_find(begin_, begin_+allocated_, value);
		*result.first = std::forward<U>(value);
		occupied_ += uint32_t(result.second == false);
		return result;
	}
	//removes element. invalidates all iterators.
	void erase(const T* element)
	{
		remove_internal(begin_, element, begin_+allocated_);
	}
	//removes element == value. invalidates all iterators.
	bool erase(const T& value)
	{
		auto b = begin_;
		auto e = begin_+allocated_;
		auto found = probe_find(b, e, value);
		if (found.second)
		{
			remove_internal(b, found.first, e);
			return true;
		}
		return false;
	}
	size_t change_tombstone(Tomb tomb_gen)
	{
		Equal equal = eq_;
		auto new_tomb = tomb_gen();
		auto tomb = tombstone();
		if (equal(new_tomb, tomb))
		{
			return 0;
		}
		size_t num_changed = 0;
		auto e = begin_ + allocated_;
		for (auto b = begin_; b != e; ++b)
		{
			if (equal(*b, tomb))
			{
				*b = new_tomb;
			}
			else if (equal(*b, new_tomb))
			{
				++num_changed;
			}
		}
		occupied_ -= num_changed;
		tomb_gen_ = std::move(tomb_gen);
		return num_changed;
	}
	decltype(auto) tombstone() const
	{
		return tomb_gen_();
	}

	bool empty() const
	{
		return occupied_ == 0;
	}

	//invalidates all iterators
	void clear()
	{
		std::fill(begin_, end_, tombstone());
		occupied_ = 0;
	}

	//returns pair:
	// location where value_ would be found if it were in the set
	// boolean denoting whether or not it actually is in the set
	auto find(const T& value) const
	{
		return probe_find(begin_, begin_+allocated_, value);
	}

	bool contains(const T& value) const
	{
		return find(value).second;
	}

	auto begin() const
	{
		return iterator(begin_, *this);
	}

	auto end() const
	{
		return iterator(begin_+allocated_, *this);
	}

	~hot_set()
	{
		stdext::destroy_a(allocator_, begin_, begin_ + allocated_);
		allocator_.deallocate(begin_, allocated_);
	}
};

template<class T, T tombstone> using hoc_set = hot_set< T, std::integral_constant<T, tombstone> >;
#if 1

//Map implementation, unique keys
//The user provides a key which shall never be inserted


template<
	class Key,	//key type
	class Value, //value type
	class Tomb,	//tombstone (key) generator function
	class Equal = std::equal_to<void>,//key comparator
	class KeyAlloc = std::allocator<Key>, //allocator
	class ValAlloc = std::allocator<Value>, //allocator
	class Hash = std::hash<Key>,//hasher
	class Load = default_load_policy//controls load factor and related concerns
>
class hot_map
{
	Key* kbegin_;
	Value* vbegin_;
	size_t allocated_;
	size_t capacity_;
	size_t occupied_;
	Hash hash_;
	Load load_alg_;
	Equal eq_;
	Tomb tomb_gen_;
	KeyAlloc kallocator_;
	ValAlloc vallocator_;

	void init(size_t size)
	{
		if (size > 0)
		{
			kbegin_ = kallocator_.allocate(size);
			vbegin_ = vallocator_.allocate(size);
			allocated_ = size;

			stdext::uninitialized_fill_a(kallocator_, kbegin_, kbegin_+size, tombstone());
			occupied_ = 0;
			capacity_ = load_alg_.occupancy(size);
		}
	}

	void rehash(size_t newsize)
	{
		auto oldallocated = allocated_;
		auto oldkbegin = kbegin_;
		auto oldvbegin = vbegin_;

		capacity_ = load_alg_.occupancy(newsize);

		auto tomb = tombstone();
		auto kb = kallocator_.allocate(newsize);
		auto ke = kb + newsize;
		auto vb = vallocator_.allocate(newsize);
		auto ve = vb + newsize;
		stdext::uninitialized_fill_a(kallocator_, kb, ke, tomb);
		
		auto oldkit = oldkbegin;
		auto oldvit = oldvbegin;

		auto equal = eq_;
		auto oldkend = oldkbegin + oldallocated;
		while (oldkit != oldkend)
		{
			auto& old_key = *oldkit;
			auto& old_val = *oldvit;
			if (!equal(old_key, tomb))
			{
				auto kpos = probe_find(kb, ke, old_key).first;
				*kpos = std::move(old_key);
				auto vpos = (kpos - kb);
				std::allocator_traits<ValAlloc>::construct(vallocator_, vb + vpos, std::move(old_val) );
			}
			++oldkit;
		}
		kallocator_.deallocate(oldkbegin, oldallocated);
		vallocator_.deallocate(oldvbegin, oldallocated);
		kbegin_ = kb;
		vbegin_ = vb;
		allocated_ = newsize;
	}
	void remove_internal(Key* first, Key* element, Key* last)
	{
		--occupied_;
		auto tomb = tombstone();
		*element = tomb;
		auto v_pos = v_begin + (element - first);
		std::allocator_traits<VAlloc>::destroy(vallocator_, v_pos);

		auto value_position = [vbegin_, first](Key* key)
		{
			return vbegin_ + (key - first);
		};
		//rehash elements that may have collided
		probe(first, element, last, [&](T& element)
		{
			auto new_key_position = probe_find(first_, last_, element).first;
			if (new_key_position != &element)
			{
				//our probe definitely found a tombstone, so the corresponding value bucket is uninitialized already.
				assert(element == tomb);
				*new_key_position = std::move(element);
				element = tomb;
				auto old_value_position = value_position(&element);
				std::allocator_traits<ValAlloc>::construct(vallocator_, value_position(new_key_position), std::move(*old_value_position));
				std::allocator_traits<ValAlloc>::destroy(vallocator_, old_value_position);
			}

		});
	}
	auto probe_find(Key* first, Key* last, const Key& search) const
	{
		auto start = load_alg_.select(first, last, hash_(search));
		return stdext::ring_find_sentinel(first, start, last, eq_, tombstone(), search);
	}

	template<class Func>
	void probe(Key* first, Key* start, Key* last, Func f) const
	{
		auto equal = eq_;
		auto tomb = tombstone();
		auto current = start_;
		for (int i = 0; i < 2; ++i)
		{
			while (current != last_)
			{
				auto& value = *current;
				if (equal(tomb, value))
				{
					return;
				}
				else
				{
					f(value);
				}
				++current;
			};
			last_ = start_;
			current = first_;
		};
	}

	void destroy_values()
	{
		auto kb = kbegin_;
		auto n = allocated_;
		auto equal = eq_;
		auto vb = vbegin_;
		auto tomb = tombstone();
		//only destroy values which were constructed (have a corresponding valid key)
		for (size_t i = 0; i < n; ++i)
		{
			if (!equal(kb[i], tomb))
			{
				std::allocator_traits<ValAlloc>::destroy(vallocator_, vb + i);
			}
		}
	}
public:
	struct iterator : std::iterator< std::forward_iterator_tag, std::pair<const Key&, Value&> >
	{
		const hot_map& map_;
		Key* kcurrent_;
		iterator(const iterator&) = default;
		iterator(iterator&&) = default;
		iterator(Key* kcurrent, const hot_map& map)
			:kcurrent_(kcurrent), map_(map)
		{
			advance();
		}

		std::pair<const Key&, Value&> operator*() const
		{
			return{ *kcurrent_, map_.vbegin_ + (kcurrent_ - map_.kbegin_) };
		}
		void advance()
		{
			auto c = tombstone_compare();
			kcurrent_ = std::find_if(kcurrent_, map_.kbegin_ + map_.allocated_, [&c](auto& elem){return !c(elem); });
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
	};

	hot_map()
		: kbegin_()
		, kend_()
		, vbegin_(),
		, vend_()
		, capacity_()
		, occupied_()
	{}

	hot_map(const hot_map& in)
		: allocator_(in.allocator_)
		, tomb_gen_(in.tomb_gen_)
		, capacity_(in.capacity_)
		, occupied_(in.occupied_)
		, hash_(in.hash_)
		, load_alg_(in.load_alg_)
		, eq_(in.eq_)
	{
		auto size = in.allocated();
		kbegin_ = kallocator_.allocate(size);
		vbegin_ = vallocator_.allocate(size);
		allocated_ = size;
		stdext:uninitialized_copy_a(kallocator_, in.kbegin_, in.kbegin_+in.allocated_, begin_);
		//todo conditionally copy values
		asdfasf;
	}

	hot_map(hot_map&& in)
		: kbegin_(in.kbegin_)
		, vbegin_(in.vbegin )
		, allocated_(in.allocated_)
		, capacity_(in.capacity_)
		, occupied_(in.occupied_)
		, Alloc(std::move(in))
		, hash(std::move(in.hash))
		, load_alg(std::move(in.load_alg))
		, eq(std::move(in.eq))
	{
		in.capacity_ = 0;
		in.occupied_ = 0;
		in.vbegin_ = nullptr;
		in.kbegin_ = nullptr;
		in.allocated_ = 0;
	}

	hot_map(size_t capacity, Tomb tombstone = {}, Hash hash = {}, Equal equal = {}, Load load = {}, ValAlloc valloc = {}, KeyAlloc kalloc = {})
		: hash_(std::move(hash))
		, load_alg_(std::move(load))
		, eq_(std::move(equal))
		, kallocator_(std::move(kalloc))
		, vallocator_(std::move(valloc))
		, tomb_gen_(std::move(tombstone))
		, occupied_(0)
		, capacity_(0)
		, kbegin_(nullptr)
		, vbegin_(nullptr)
		, allocated_(0)
	{
		init(load_alg_.allocated(capacity));
	}

	hot_map& operator=(const hot_map& other)
	{
		~hot_map();
		return *new(this) hot_map(other);
	}
	hot_map& operator=(hot_map&& other)
	{
		~hot_map();
		return *new(this) hot_map(std::move(other));
	}
	bool is_tombstone(const Key& key) const
	{
		return eq_(tombstone(), key);
	}

	//number of elements allocated by the set
	size_t allocated() const
	{
		return allocated_;
	}

	//number of elements the set may contain before reallocating
	size_t capacity() const
	{
		return capacity_;
	}

	//number of elements in the set
	size_t size() const
	{
		return occupied_;
	}

	span<Key> raw_key_span() const
	{
		return{ kbegin_, kbegin_+allocated_ };
	}
	span<Value> raw_value_span()
	{
		return{ vbegin_, vbegin_ + allocated_ };
	}

	span<const Value> raw_value_span() const
	{
		return{ vbegin_, vbegin_ + allocated_ };
	}
	void shrink()
	{
		auto target_size = load_alg.allocated(capacity_);
		if (target_size != allocated_)
		{
			rehash(target_size);
		}
	}

	//Inserts an element into the set
	//If size() == capacity(), invalidates any iterators
	template<class K, class V>
	auto insert(K&& key, V&& value)
	{
		if (capacity_ == occupied_)
		{
			rehash(load_alg_.grow(allocated_));
		}
		return stable_insert(std::forward<K>(key), std::forward<V>(value));
	}

	//Inserts an element into the set
	//precondition: size < capacity
	//invalidates no iterators
	template<class K, class V>
	auto stable_insert(K&& key, V&& value)
	{
		auto result = probe_find(kbegin_, kbegin_+allocated_, key);
		auto v_pos = vbegin_ + (result.first - kbegin_);
		if (is_tombstone(*result.first))
		{
			std::allocator_traits<ValAlloc>::construct(vallocator_, v_pos, std::forward<V>(value));		
		}
		else
		{ 
			*(v_pos) = std::forward<V>(value);
		}
		*result.first = std::forward<K>(key);
		occupied_ += uint32_t(result.second == false);
		return result;
	}
	//removes element. invalidates all iterators.
	void erase(iterator element)
	{
		remove_internal(kbegin_, element, kbegin_+allocated_);
	}
	//removes element == value. invalidates all iterators.
	bool erase(const Key& key)
	{
		auto b = kbegin_;
		auto e = kbegin_+allocated_;
		auto found = probe_find(b, e, key);
		if (found.second)
		{
			remove_internal(b, found.first, e);
		}
		return found.second;
	}
	size_t change_tombstone(Tomb tomb_gen)
	{
		auto b = kbegin_;
		auto n = allocated_;
		auto v = vbegin_;
		Equal equal = eq_;
		auto new_tomb = tomb_gen();
		auto tomb = tombstone();
		size_t num_changed = 0;
		if (equal(new_tomb, tomb))
		{
			return 0;
		}

		for (size_t i = 0; i < n; ++i)
		{
			if (equal(b[i], tomb))
			{
				b[i] = new_tomb;
			}
			else if (equal(b[i], new_tomb))
			{
				++num_changed;
				std::allocator_traits<ValAlloc>::destroy(vallocator_, v + i); //value is no longer valid, must destroy it
			}
		}
		occupied_ -= num_changed;
		tomb_gen_ = std::move(tomb_gen);
		return num_changed;
	}
	constexpr decltype(auto) tombstone() const noexcept(noexcept(tomb_gen_()))
	{
		return tomb_gen_();
	}

	bool empty() const
	{
		return occupied_ == 0;
	}

	//invalidates all iterators
	bool clear()
	{
		destroy_values();
		std::fill(kbegin_, kbegin_ + allocated_, tombstone());
		occupied_ = 0;
	}

	//returns pair:
	// location where value_ would be found if it were in the set
	// boolean denoting whether or not it actually is in the set
	auto find(const Key& key) const
	{
		return probe_find(begin_, end_, key);
	}

	bool contains(const Key& key) const
	{
		return find(key).second;
	}

	auto begin() const
	{
		return iterator(begin_, *this);
	}

	auto end() const
	{
		return iterator(end_, *this);
	}

	~hot_map()
	{
		auto n = allocated_;
		destroy_values();
		stdext::destroy_a(kallocator_, kbegin_, kbegin_+n);
		kallocator_.deallocate(kbegin_, n);
		vallocator_.deallocate(vbegin_, n);
	}
};


template<class K, class V, K tombstone> using hoc_map = hot_map< K, V, std::integral_constant<K, tombstone>>;

#endif

#if 0

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



//template<class K, class V>
//using hoc_multimap = hoc_set< std::pair<K,V>, std::pair<K,V>{} > ;