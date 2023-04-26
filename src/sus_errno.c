#include "sus_errno.h"

int sus_errno; // extern

int sus_set_errno(int val)
{
	sus_errno = val;
	return 0;
}
