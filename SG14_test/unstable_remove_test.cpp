#include "SG14_test.h"
#include <vector>
#include <array>
#include <ctime>
#include <iostream>
#include <algorithm>
#include "algorithm_ext.h"
#include <cassert>
#include <memory>
#include <chrono>

static void print_results(const char* name, std::vector<unsigned long>& results)
{
	std::cout << name << " : ";
	for (auto& val : results)
	{
		std::cout << val;
		std::cout << ",";
	}
	std::cout << std::endl;
}
template<size_t N>
struct do_tests
{
	static const size_t test_runs = 200;
	
	auto makelist(size_t count, size_t remove_count)
	{
		std::vector<std::array<unsigned char, N>> list(count);
		remove_count = std::min(remove_count, list.size());
		std::fill_n(list.begin(), remove_count, std::array<unsigned char, N>{ 1 });
		std::random_shuffle(list.begin(), list.end());
		return list;
	}
	do_tests()
	{

		auto time = [&](int count, size_t remove_count, auto&& f)
		{
			auto list = makelist(count, remove_count);
			auto t0 = std::chrono::high_resolution_clock::now();
			f(list);
			auto t1 = std::chrono::high_resolution_clock::now();
			return (t1 - t0).count();
		};
		auto cmp = [](auto& f) {return f[0] & 1; };
		auto partitionfn = [&](auto& f)
		{
			stdext::partition(f.begin(), f.end(), cmp);
		};
		auto unstablefn = [&](auto& f)
		{
			stdext::unstable_remove_if(f.begin(), f.end(), cmp);
		};
		auto removefn = [&](auto& f)
		{
			stdext::remove_if(f.begin(), f.end(), cmp);
		};

		std::vector< unsigned long> partition_results, unstable_remove_if_results, remove_if_results;
		auto gen_count = 2000;

		std::cout << "Test @ " << N << std::endl;
		for (int rem_count = gen_count; rem_count >= 0; rem_count -= 100)
		{
			std::vector< unsigned long > partition, unstable_remove_if, remove_if;

			partition.reserve(test_runs);
			unstable_remove_if.reserve(test_runs);
			remove_if.reserve(test_runs);
			for (int i = 0; i < test_runs; ++i)
			{
				remove_if.push_back(time(gen_count, rem_count, removefn));
				unstable_remove_if.push_back(time(gen_count, rem_count, unstablefn));
				partition.push_back(time(gen_count, rem_count, partitionfn));
			}

			auto median = [](std::vector<unsigned long>& v)
			{
				auto med = (v.begin() + v.size()/ 2) ;
				std::nth_element(v.begin(), v.begin() + (v.size()/2), v.end());
				return *med;
			};

			partition_results.push_back( median(partition) );
			unstable_remove_if_results.push_back( median(unstable_remove_if) );
			remove_if_results.push_back( median(remove_if) );
		}


		print_results("partition", partition_results);
		print_results("unstable", unstable_remove_if_results);
		print_results("remove_if", remove_if_results);
	}
	


};

void sg14_test::unstable_remove_test()
{
	{ do_tests<1> x1;	  }
	{ do_tests<2> x2;	  }
	{ do_tests<4> x4;	  }
	{ do_tests<8> x8;	  }
	{ do_tests<16> x16;	  }
	{ do_tests<32> x32; }
	{ do_tests<64> x64;	  }
	{ do_tests<128> x128;  }
	{ do_tests<256> x256; }

	std::cin.get();
}
