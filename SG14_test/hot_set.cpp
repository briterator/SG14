#include "SG14_test.h"
#include <cassert>
#include "hot_set.h"
#include <iostream>
#include <chrono>
#include <set>
#include <vector>
#include <unordered_set>
#include <fstream>
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
		}
		for (int i = 101; i < 2000; ++i)
		{
			assert(!set.contains(i));
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

	void hotmap_each_test()
	{
#if 0
		hov_map<int, const char*> english_text;
		english_text.insert(5, "five");
		english_text.insert(33, "thirty three");
		english_text.insert(6, "six");
		english_text.insert(2, "two");
		english_text.insert(1, "one");
		english_text.erase(5);
		assert(english_text.contains(6));
		assert(english_text.contains(1));
#endif
	}

	void hotmultimap_each_test()
	{
#if 0
		auto foo = hod_multimap<int, const char*>( 8,  -1, (const char*)nullptr );
		foo.insert(3, "3");
		foo.insert(3, "three");
		foo.insert(3, "tres");
		foo.insert(3, "drei");

		foo.insert(5, "five");
		foo.insert(5, "funf");

		auto a = "two hundred";
		foo.insert(200, a);
		foo.insert(3893, "three eight nine three");
		assert(foo.find(200, a).second);

		for (auto& elem : foo)
		{
			std::cout << elem.key << "=" << elem.value << std::endl;
		}
#endif
	}
	template<class Generator>
	auto do_set_test(int i, Generator g)
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		auto a = g(i);
		for (int j = 0; j < i; ++j)
		{
			a.insert(j+j);
		}
		return (std::chrono::high_resolution_clock::now() - t0).count();
	}
	auto do_vec_test(int i)
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		auto a = std::vector<int>(i);
		for (int j = 0; j < i; ++j)
		{
			a.push_back(j+j);
		}
		return (std::chrono::high_resolution_clock::now() - t0).count();
	}
	template<class T>
	void save_timing(const char* file, T begin, T end)
	{
		std::ofstream out(file);
		while (begin != end)
		{
			out << *begin;
			out << ",";
			++begin;
		}
	}
	void perf_tests()
	{
		size_t N = 10000;
		std::vector<uint32_t> unorderedsettimes;
		std::vector<uint32_t> settimes;
		std::vector<uint32_t> hocsettimes;
		std::vector<uint32_t> hovsettimes;
		std::vector<uint32_t> vectimes;
		for (size_t i = 240; i < N; i+=10)
		{
			unorderedsettimes.push_back( do_set_test(i, [](size_t N) {return std::unordered_set<int>(N); }) );
			hocsettimes.push_back( do_set_test(i, [](size_t N) { return hoc_set<int, -1>(N); }) );
			hovsettimes.push_back( do_set_test(i, [](size_t N) { return hov_set<int>(N, -1); }) );
			settimes.push_back( do_set_test(i, [](size_t N) { return std::set<int>(); }) );
			vectimes.push_back(do_vec_test(i));
		}
		save_timing("vec.csv", vectimes.begin(), vectimes.end());
		save_timing("set.csv", settimes.begin(), settimes.end());
		save_timing("hoc_set.csv", hocsettimes.begin(), hocsettimes.end());
		save_timing("hov_set.csv", hovsettimes.begin(), hovsettimes.end());
		save_timing("unordered.csv", unorderedsettimes.begin(), unorderedsettimes.end());
	}

	void hotset()
	{

		auto dyset = [] {return hov_set<int>{32, -1 }; }; //hotset with runtime tombstone
		auto stset = [] {return hoc_set<int, -1>{ 64, }; }; //hotset with compile-time tombstone

		hotmap_each_test();
		hotmultimap_each_test();
		
		hotset_each_test(dyset);
		hotset_each_test(stset);


		perf_tests();
	}
}
