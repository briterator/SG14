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
	//this class serves to compare the cost of push_back vs. set insertion
	//the theory is that inserting into a set is unlikely to ever be faster than push_back
	template<class T>
	struct vector_set_adapter
	{
		std::vector<T> data;
		vector_set_adapter(size_t N)
			:data(N)
		{
		}
		vector_set_adapter(const vector_set_adapter&) = default;
		vector_set_adapter(vector_set_adapter&&) = default;
		void insert(T i)
		{
			data.push_back(i);
		}
		void erase(T i)
		{
			data.pop_back();
		}
	};

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
		auto median = measured.begin() + 4;
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
	template<class T>
	bool contains(hov_set<T>& a, T val)
	{
		return a.find(val).second;
	}
	template<class T>
	bool contains(vector_set_adapter<T>& a, T val)
	{
		return a.data.size() > 0 && a.data.back() == val;
	}
	template<class T, T i>
	bool contains(hoc_set<T, i>& a, T val)
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
		size_t N = 10000;
		std::vector<uint32_t> unorderedsettimes;
		std::vector<uint32_t> settimes;
		std::vector<uint32_t> hocsettimes;
		std::vector<uint32_t> hovsettimes;
		std::vector<uint32_t> vectimes;
		for (size_t i = 0; i < N; i+=500)
		{
			unorderedsettimes.push_back( test(i, [](size_t N) {return std::unordered_set<int>(N); }) );
			hocsettimes.push_back( test(i, [](size_t N) { return hoc_set<int, -1>(N); }) );
			hovsettimes.push_back( test(i, [](size_t N) { return hov_set<int>(N, -1); }) );
			settimes.push_back( test(i, [](size_t N) { return std::set<int>(); }) );
			vectimes.push_back(test(i, [](size_t N) { return vector_set_adapter<int>(N); }));
		}
		std::ofstream out(file);
		out << "vec, "; save_timing(out, vectimes.begin(), vectimes.end());
		out << "set, "; save_timing(out, settimes.begin(), settimes.end());
		out << "hoc, "; save_timing(out, hocsettimes.begin(), hocsettimes.end());
		out << "hov, "; save_timing(out, hovsettimes.begin(), hovsettimes.end());
		out << "uno, "; save_timing(out, unorderedsettimes.begin(), unorderedsettimes.end());
	}

	void hotset_change_tombstone_test()
	{
		hov_set<int> a(100, 0);
		a.insert(1);
		a.insert(2);
		a.insert(100);
		assert(a.size() == 3);

		a.change_tombstone(1);
		assert(a.size() == 2);
	}

	void hotset()
	{

		auto dyset = [] {return hov_set<int>{32, -1 }; }; //hotset with runtime tombstone
		auto stset = [] {return hoc_set<int, -1>{ 64, }; }; //hotset with compile-time tombstone

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
