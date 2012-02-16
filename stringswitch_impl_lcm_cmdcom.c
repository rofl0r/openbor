typedef enum {
	stringswitch_enumerator_default_member_name(lcm_cmdcom),
	stringswitch_enumerator_member_name(lcm_cmdcom, u),
	stringswitch_enumerator_member_name(lcm_cmdcom, d),
	stringswitch_enumerator_member_name(lcm_cmdcom, f),
	stringswitch_enumerator_member_name(lcm_cmdcom, b),
	stringswitch_enumerator_member_name(lcm_cmdcom, a),
	stringswitch_enumerator_member_name(lcm_cmdcom, j),
	stringswitch_enumerator_member_name(lcm_cmdcom, s),
	stringswitch_enumerator_member_name(lcm_cmdcom, k),
	stringswitch_enumerator_member_name(lcm_cmdcom, a2),
	stringswitch_enumerator_member_name(lcm_cmdcom, a3),
	stringswitch_enumerator_member_name(lcm_cmdcom, a4),
} stringswitch_enumerator_name(lcm_cmdcom);

static int stringswitch_enumerator_eval_func(lcm_cmdcom) (char* str, size_t l) {
	switch(l) {
		case 1:
			switch(str[0]) {
				case 'u':
					return stringswitch_enumerator_member_name(lcm_cmdcom, u);
				case 'd':
					return stringswitch_enumerator_member_name(lcm_cmdcom, d);
				case 'f':
					return stringswitch_enumerator_member_name(lcm_cmdcom, f);
				case 'b':
					return stringswitch_enumerator_member_name(lcm_cmdcom, b);
				case 'a':
					return stringswitch_enumerator_member_name(lcm_cmdcom, a);
				case 'j':
					return stringswitch_enumerator_member_name(lcm_cmdcom, j);
				case 's':
					return stringswitch_enumerator_member_name(lcm_cmdcom, s);
				case 'k':
					return stringswitch_enumerator_member_name(lcm_cmdcom, k);
				default: goto main_default;
			}
		case 2:
			if(str[0]!='a') goto main_default;
			switch(str[1]) {
				case '2':
					return stringswitch_enumerator_member_name(lcm_cmdcom, a2);
				case '3':
					return stringswitch_enumerator_member_name(lcm_cmdcom, a3);
				case '4':
					return stringswitch_enumerator_member_name(lcm_cmdcom, a4);
				default: goto main_default;
			}
		default:
			main_default:
			return stringswitch_enumerator_default_member_name(lcm_cmdcom);
	}
}
