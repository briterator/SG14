#if defined(_MSC_VER)
#include <SDKDDKVer.h>
#endif

#include <stdio.h>

#include "SG14_test.h"

int main(int, char *[])
{
	//sg14_test::await_alg_test();
	//sg14_test::rolling_queue_test();
	//sg14_test::unstable_remove_test();
	//sg14_test::uninitialized();
	//sg14_test::fixed_point_test();
	//sg14_test::plf_test_suite();
	sg14_test::hotset();

	puts("tests completed");

	return 0;
}

