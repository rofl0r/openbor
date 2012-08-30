#ifndef STRINGSWITCH_H
#define STRINGSWITCH_H

#include <string.h>

#define stringswitch_enumerator_name(N) strswitch_enumerator_##N
#define stringswitch_enumerator_member_name(N, X) strswitch_enumerator_##N##_member_##X
#define stringswitch_enumerator_length_name(N) strswitch_enumerator_##N##_length
#define stringswitch_enumerator_value_name(N) strswitch_enumerator_##N##_value
#define stringswitch_enumerator_eval_func(N) strswitch_enumerator_eval_func_##N
#define stringswitch_enumerator_default_member_name(N) stringswitch_enumerator_member_name(N, DEFAULT__________)

#define get_stringswitch_value(N, X, L) stringswitch_enumerator_eval_func(N)((X), (L))
#define stringswitch_l(N, X, L) int stringswitch_enumerator_value_name(N) = stringswitch_enumerator_eval_func(N)((X), (L)); \
	switch(stringswitch_enumerator_value_name(N))
		
// for "dynamic" strings, i.e. non-literals
#define stringswitch_d(N, X) size_t stringswitch_enumerator_length_name(N) = strlen(X); stringswitch_l(N, X, stringswitch_enumerator_length_name(N))
// for static stringliterals
#define stringswitch_s(N, X) stringswitch_l(N, X, sizeof(X) - 1)
		
#define stringcase(N, X) case stringswitch_enumerator_member_name(N, X)
#define stringcase_default(N) case stringswitch_enumerator_default_member_name(N): default

#endif