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
		auto tombstone_comp = set.tombstone_compare();
		assert(tombstone_comp(-1));
		for (int i = 0; i < 100; ++i)
		{
			auto x = rand();
			set.insert(x);
			set.erase(x);
		}
		assert(set.size() == 0);
		for (auto& elem : set.raw_span())
		{
			assert(tombstone_comp(elem));
		}
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
	void hotset_test_4(T set)
	{
		for (int i = 0; i < 500; ++i)
		{
			set.insert(i);
		}
		for (int i = 499; i >=400; --i)
		{
			set.erase(i);
		}
		assert(set.size() == 400);
		for (auto& elem : set)
		{
			assert(elem < 400);
			assert(elem >= 0);
		}
		for (int i = 0; i < 400; ++i)
		{
			set.erase(i);
		}
		assert(set.size() == 0);
		for (auto& elem : set.raw_span())
		{
			assert(elem == -1);
		}
	}
	template<class T>
	void hotset_each_test(T func)
	{
		hotset_test_1(func());
		hotset_test_2(func());
		hotset_test_3(func());
		hotset_test_4(func());
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
	
	template<class Predicate>
	auto time_median(Predicate p)
	{
		std::vector<uint64_t> measured;
		for (int test = 0; test < 10; ++test)
		{
			auto t0 = std::chrono::high_resolution_clock::now();
			p();
			measured.push_back((std::chrono::high_resolution_clock::now() - t0).count());
		}
		auto median = measured.begin() + (measured.size()/2);
		std::nth_element(measured.begin(), median, measured.end());
		return *median;
	}

	template<class Generator>
	auto set_insert_test(int i, Generator g)
	{
		return time_median([&]
		{
			auto a = g(i);
			for (int j = 0; j < i; ++j)
			{
				a.insert(j + j);
			}
		});
	}

	template<class Generator>
	auto set_erase_test(int i, Generator g)
	{
		return time_median([&]
		{
			auto a = g(i);
			for (int j = 0; j < i; ++j)
			{
				a.insert(j + j);
				a.erase(j + j);
			}
		});
	}
	template<class T>
	bool contains(std::set<T>& a, T val)
	{
		return a.find(val) != a.end();
	}

	template<class T>
	bool contains(std::unordered_set<T>& a, T val)
	{
		return a.find(val) != a.end();
	}
	template<class T, class U>
	bool contains(hot_set<T, U>& a, T val)
	{
		return a.find(val).second;
	}

	template<class Generator>
	auto set_abuse_test(int i, Generator g)
	{
		return time_median([&]
		{
			auto a = g(i);
			for (int j = 0; j < i; ++j)
			{
				a.insert(j + 2);
				if (contains(a, j))
				{
					a.erase(j);
				}
			}
		});
	}

	template<class T, class OUT>
	void save_timing(OUT& out, T begin, T end)
	{
		while (begin != end)
		{
			out << *begin;
			out << ",";
			++begin;
		}
		out << "\n";
	}

	template<class TEST>
	void uniform_perf_test(const char* file, TEST test)
	{
		int32_t N = 10000;
		std::vector<uint64_t> unorderedsettimes;
		std::vector<uint64_t> settimes;
		std::vector<uint64_t> hocsettimes;
		std::vector<uint64_t> hovsettimes;
		for (int32_t i = 0; i < N; i+=500)
		{
			unorderedsettimes.push_back( test(i, [](size_t N) {return std::unordered_set<int>(N); }) );
			hocsettimes.push_back( test(i, [](size_t N) { return hoc_set<int, -1>(N); }) );
			hovsettimes.push_back( test(i, [](size_t N) { return hot_set<int>(N, -1); }) );
			settimes.push_back( test(i, [](size_t N) { return std::set<int>(); }) );
			
		}
		std::ofstream out(file);
		out << "set, "; save_timing(out, settimes.begin(), settimes.end());
		out << "hoc_set, "; save_timing(out, hocsettimes.begin(), hocsettimes.end());
		out << "hot_set, "; save_timing(out, hovsettimes.begin(), hovsettimes.end());
		out << "unordered_set, "; save_timing(out, unorderedsettimes.begin(), unorderedsettimes.end());
	}

	void hotset_change_tombstone_test()
	{
		hot_set<int> a(100, 0);
		a.insert(1);
		a.insert(2);
		a.insert(100);
		assert(a.size() == 3);

		a.change_tombstone(1);
		assert(a.size() == 2);
	}

	void hotset()
	{

		auto dyset = [] {return hot_set<int>{32, -1 }; }; //hotset with runtime tombstone
		auto stset = [] {return hoc_set<int, -1>{ 64 }; }; //hotset with compile-time tombstone

		hotmap_each_test();
		hotmultimap_each_test();
		
		hotset_each_test(dyset);
		hotset_each_test(stset);

		hotset_change_tombstone_test();

		uniform_perf_test("insert_perf.csv",[](auto&&... As) {return set_insert_test(As...); });
		uniform_perf_test("erase_perf.csv", [](auto&&... As) {return set_erase_test(As...); });
		uniform_perf_test("abuse.csv", [](auto&&... As) {return set_abuse_test(As...); });
	}
}


namespace sg14_test
{

	void hotmap()
	{
		hoc_map<int, int, 0> data(64);
		data.insert( 1, 100 );
		data.insert(2, -3);
	}
}