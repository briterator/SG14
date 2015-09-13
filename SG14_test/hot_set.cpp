#include "SG14_test.h"
#include <cassert>
#include "hot_set.h"
#include <iostream>

namespace sg14_test
{
	template<class T>
	void hotset_test_1(T set)
	{
		auto tombstone = set.tombstone();
		assert(tombstone == -1);
		for (int i = 0; i < 100; ++i)
		{
			auto x = rand();
			set.insert(x);
			set.erase(x);
		}
		assert(set.size() == 0);
	}

	template<class T>
	void hotset_test_2(T set)
	{
		for (int i = 0; i < 100; ++i)
		{
			set.insert(i);
			assert(set.contains(i));
		}
		assert(set.size() == 100);
		for (auto& elem : set)
		{
			assert(elem < 100 && elem >= 0);
		}
		for (int i = 0; i < 100; ++i)
		{
			assert(set.contains(i));
			assert(set.find(i) != set.end());
		}
		for (int i = 101; i < 2000; ++i)
		{
			assert(!set.contains(i));
			assert(set.find(i) == set.end());
		}
	}

	template<class T>
	void hotset_test_3(T set)
	{
		for (int i = 0; i < 100; ++i)
		{
			set.insert(i);
		}
		auto other = set;
		for (auto& elem : set)
		{
			assert(other.contains(elem));
		}
		for (auto& elem : other)
		{
			assert(set.contains(elem));
		}
	}
	template<class T>
	void hotset_each_test(T func)
	{
		hotset_test_1(func());
		hotset_test_2(func());
		hotset_test_3(func());
	}

	void hotset()
	{
		auto dyset = [] {return hod_set<int>{-1, 32};}; //hotset with runtime tombstone
		auto stset = [] {return hos_set<int, -1>{ {}, 64}; }; //hotset with compile-time tombstone
		
		hotset_each_test(dyset);
		hotset_each_test(stset);

		std::cin.get();
	}
}
