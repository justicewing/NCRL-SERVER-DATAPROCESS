#include"ldpc.h"

#include <stdlib.h>

int main()
{
	nr15_ldpc_t *ldpc_arg;
	ldpc_arg = (nr15_ldpc_t*)malloc(sizeof(nr15_ldpc_t));
	int B = 15000;
	double code_rate = 0.5;
	nr15_fec_ldpc_param_init(ldpc_arg, B, code_rate);

	return 0;
}