#include "SG14_test.h"
#include <cassert>
#include "hotset.h"
#include <iostream>

namespace sg14_test
{
	void hotset()
	{

		hodtset<int> foo(-1);
		for (int i = 0; i < 100; ++i)
		{
			auto x = rand();
			if (x != -1)
			{
				foo.insert(x);
			}

		}

		for (auto& val : foo)
		{
			std::cout << val;
		}
	}
}
