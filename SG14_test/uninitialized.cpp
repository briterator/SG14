#include "SG14_test.h"
#include <vector>
#include <array>
#include <ctime>
#include <iostream>
#include <algorithm>
#include "algorithm_ext.h"
#include <cassert>
#include <memory>

namespace
{
	struct lifetest
	{
		static uint64_t construct;
		static uint64_t destruct;
		static uint64_t move;
		lifetest()
		{
			++construct;
		}
		lifetest(lifetest&& /*in*/)
		{
			++move;
		}
		~lifetest()
		{
			++destruct;
		}
		static void reset()
		{
			construct = 0;
			destruct = 0;
			move = 0;
		}

		static void test(uint64_t inconstruct, uint64_t indestruct, uint64_t inmove)
		{
			assert(construct == inconstruct);
			assert(destruct == indestruct);
			assert(move == inmove);
		}
	};
	uint64_t lifetest::construct;
	uint64_t lifetest::destruct;
	uint64_t lifetest::move;
	std::vector<int> bar() {
		return{};
	};
	void value()
	{
		for (auto n = 0; n < 256; ++n)
		{
			auto m = (lifetest*)malloc(sizeof(lifetest) * n);
			lifetest::test(0, 0, 0);
			stdext::uninitialized_value_construct(m, m + n);
			lifetest::test(n, 0, 0);
			stdext::destroy(m, m + n);
			lifetest::test(n, n, 0);
			free(m);
			lifetest::reset();
		}
		auto x = &bar();
		auto m = (int*)malloc(sizeof(int) * 5);
		stdext::uninitialized_value_construct(m, m + 5);
		assert(std::all_of(m, m + 5, [](int x) { return x == 0; }));
		free(m);
	};

	void def()
	{
		for (auto n = 0; n < 256; ++n)
		{
			auto mem1 = (lifetest*)malloc(sizeof(lifetest) * n);
			lifetest::test(0, 0, 0);
			stdext::uninitialized_default_construct(mem1, mem1 + n);
			lifetest::test(n, 0, 0);

			auto mem2 = (lifetest*)malloc(sizeof(lifetest) * n);
			stdext::uninitialized_move(mem1, mem1 + n, mem2);
			lifetest::test(n, 0, n);
			stdext::destroy(mem2, mem2 + n);
			lifetest::test(n, n, n);
			free(mem1);
			free(mem2);
			lifetest::reset();
		}
	}
}

template<class T, class U>
void* operator new(size_t s, std::raw_storage_iterator<T, U> it) noexcept
{
	return ::operator new(s, it.base());
}

template<class T, class U>
void operator delete (void* m, std::raw_storage_iterator<T, U> it) noexcept
{
	return ::operator delete(m, it.base());
}

template<class T>
auto make_storage_iterator(T&& iterator)
{
	return std::raw_storage_iterator<std::remove_reference<T>::type, decltype(*iterator)>(std::forward<T>(iterator));
}
template<typename T>
struct TScopedLambda
{
	TScopedLambda(const T& InLambda) : Lambda(InLambda) { }

	~TScopedLambda() { Lambda(); }

private:
	const T& Lambda;
};

#define SCOPED_LAMBDA(L) auto ScopedLambda = L; TScopedLambda<decltype(ScopedLambda)> Kaboom(ScopedLambda);

void sg14_test::uninitialized()
{
	//value();
	//def();
	auto asdff = [](int, int) {};
	SCOPED_LAMBDA([&] 
	{
		asdff(0 , 0);  
	});

	uint32_t b= 0xFFFFFFFF;
	unsigned char a = b;
	std::aligned_storage_t<16384, 8> foo;
	std::vector<int>* foo_begin = (std::vector<int>*)&foo;
	auto it = make_storage_iterator(foo_begin);
	std::raw_storage_iterator<std::vector<int>*, std::vector<int>> it2(foo_begin + 30);
	std::generate_n(it, 30, [] {return std::vector<int>{3}; });
	new(it) std::vector<int>{3};
}