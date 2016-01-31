#include "SG14_test.h"
#include <vector>
#include <array>
#include <numeric>
#include <ctime>
#include <iostream>
#include <functional>
#include <algorithm>
#include "algorithm_ext.h"
#include <cassert>
#include <fstream>
#include <memory>
#include <chrono>

auto not_one = [](auto& elem) {return elem[0] != 1; };
auto is_one = [](auto& elem) {return elem[0] == 1; };

auto is_even = [](auto& elem) {return (elem[0] & 1) == 0; };
auto is_odd = [](auto& elem) {return (elem[0] & 1) == 1;  };

struct timings
{
	std::vector<int64_t> partition, remove_if, semistable, stable_partition, unstable_remove;
	void init(size_t init_size)
	{
		partition.reserve(init_size);
		unstable_remove.reserve(init_size);
		remove_if.reserve(init_size);
		semistable.reserve(init_size);
		stable_partition.reserve(init_size);
	}
	void add_median(timings& other)
	{
		auto median = [](std::vector<int64_t>& v)
		{
			auto med = (v.begin() + v.size() / 2);
			std::nth_element(v.begin(), med, v.end());
			return *med;
		};

		partition.push_back(median(other.partition));
		unstable_remove.push_back(median(other.unstable_remove));
		remove_if.push_back(median(other.remove_if));
		semistable.push_back(median(other.semistable));
		stable_partition.push_back(median(other.stable_partition));
	}
	void print(std::ostream& out)
	{
		auto print_results = [&](const char* name, std::vector<int64_t>& results)
		{
			out << name << ", ";
			for (auto& val : results)
			{
				out << val;
				out << ",";
			}
			out << std::endl;
		};

		print_results("partition", partition);
		print_results("unstable", unstable_remove);
		print_results("remove_if", remove_if);
		print_results("semistable_partition", semistable);
		print_results("stable_partition", stable_partition);
	}
};

template<size_t N>
struct do_tests
{
	static const size_t test_runs = 200;
	
	auto makearray(size_t count, size_t remove_count)
	{
		std::vector<std::array<unsigned char, N>> list(count);
		remove_count = std::min(remove_count, list.size());
		std::fill_n(list.begin(), remove_count, std::array<unsigned char, N>{ 1 });
		std::random_shuffle(list.begin(), list.end());
		return list;
	}
	do_tests(std::ostream& out)
	{
		auto time = [&](int count, size_t remove_count, auto&& f)
		{
			auto array = makearray(count, remove_count);
			auto t0 = std::chrono::high_resolution_clock::now();
			f(array);
			auto t1 = std::chrono::high_resolution_clock::now();
			return (t1 - t0).count();
		};

		auto partitionfn =   [&](auto& f){ stdext::partition(f.begin(), f.end(), not_one); };
		auto unstablefn =    [&](auto& f){ stdext::unstable_remove_if(f.begin(), f.end(), is_one);};
		auto removefn =      [&](auto& f){ stdext::remove_if(f.begin(), f.end(), is_one);};
		auto semistable_fn = [&](auto& f){ stdext::semistable_partition(f.begin(), f.end(), not_one);};
		auto stablepart_fn = [&](auto& f){ std::stable_partition(f.begin(), f.end(), not_one);};

		timings output;
		auto gen_count = 2000;

		out << "Test @ " << N << std::endl;
		for (int rem_count = gen_count; rem_count >= 0; rem_count -= 100)
		{
			timings measurements;
			measurements.init(test_runs);

			for (int i = 0; i < test_runs; ++i)
			{
				measurements.remove_if.push_back(time(gen_count, rem_count, removefn));
				measurements.unstable_remove.push_back(time(gen_count, rem_count, unstablefn));
				measurements.partition.push_back(time(gen_count, rem_count, partitionfn));
				measurements.semistable.push_back(time(gen_count, rem_count, semistable_fn));
				measurements.stable_partition.push_back(time(gen_count, rem_count, stablepart_fn));
			}

			output.add_median(measurements);
		}

		output.print(out);
	}
	

};

template<size_t ARRAY_N, class P >
int64_t iota_test_n(size_t N, P func)
{
	std::vector<std::array<int, ARRAY_N>> data(N);
	int i = 0;
	for (auto& elem : data)
	{
		elem[0] = i++;
	}

	auto t0 = std::chrono::high_resolution_clock::now();
	func(data);
	auto t1 = std::chrono::high_resolution_clock::now();
	return (t1 - t0).count();
}

template<size_t ARRAY_N>
void iota_test(std::ostream& out)
{

	auto partitionfn =   [&](auto& f){stdext::partition(f.begin(), f.end(), is_even); };
	auto unstablefn =    [&](auto& f){stdext::unstable_remove_if(f.begin(), f.end(), is_odd); };
	auto removefn =      [&](auto& f){stdext::remove_if(f.begin(), f.end(), is_odd); };
	auto semistable_fn = [&](auto& f){stdext::semistable_partition(f.begin(), f.end(), is_even);};
	auto stablepart_fn = [&](auto& f){std::stable_partition(f.begin(), f.end(), is_even);};
	out << std::endl << "iota test " << ARRAY_N << std::endl;
	timings output;
	for (int i = 0; i < 20000; i += 50)
	{
		timings measurements;
		measurements.init(i);
		for (int test_run = 0; test_run < 100; ++test_run)
		{
			measurements.unstable_remove.push_back(iota_test_n<ARRAY_N>(i, unstablefn));
			measurements.remove_if.push_back(iota_test_n<ARRAY_N>(i, removefn));
			measurements.partition.push_back(iota_test_n<ARRAY_N>(i, partitionfn));
			measurements.semistable.push_back(iota_test_n<ARRAY_N>(i, semistable_fn));
			measurements.stable_partition.push_back(iota_test_n<ARRAY_N>(i, stablepart_fn));
		}

		output.add_median(measurements);
	}
	output.print(out);

}
void sg14_test::unstable_remove_test()
{
	std::ofstream file_out("results.txt");
	{ auto x1 = do_tests<1>(file_out);	  }
	//{ do_tests<2> x2;	  }
	//{ do_tests<4> x4;	  }
	//{ do_tests<8> x8;	  }
	//{ do_tests<16> x16;	  }
	//{ do_tests<32> x32; }
	//{ do_tests<64> x64;	  }
	{ auto x128 = do_tests<32>(file_out);  }
	//{ do_tests<256> x256; }

	iota_test<1>(file_out);
	iota_test<32>(file_out);
}
