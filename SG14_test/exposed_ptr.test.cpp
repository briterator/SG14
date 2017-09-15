#include "exposed_ptr.h"
#include "SG14_test.h"
#include <cassert>
namespace
{
	struct exposed_class : enable_soft_from_this
	{
		int32_t value = 0;
		void do_thing()
		{
			auto l = soft_from(this);
		}
	};
}
namespace sg14_test
{
	void exposed_ptr_test()
	{
		{
			auto e = exposed_ptr<exposed_class>::make();
			e->do_thing();
			assert(e->value == 0);
			(*e).value = 5;
			assert(e->value == 5);
		}
		soft_ptr<int> m;
		{
			auto e = exposed_ptr<int>::make(4);
			*e = 3;

			{

				auto l = e.soft();
				m = l;
				*m = 5;
				assert(*m == 5);
			}

		}
		assert(m == nullptr);
		assert(!m);

	}
}