typedef enum {
	stringswitch_enumerator_default_member_name(lcm_cmdtype),
	stringswitch_enumerator_member_name(lcm_cmdtype, npc),
	stringswitch_enumerator_member_name(lcm_cmdtype, none),
	stringswitch_enumerator_member_name(lcm_cmdtype, item),
	stringswitch_enumerator_member_name(lcm_cmdtype, trap),
	stringswitch_enumerator_member_name(lcm_cmdtype, text),
	stringswitch_enumerator_member_name(lcm_cmdtype, enemy),
	stringswitch_enumerator_member_name(lcm_cmdtype, pshot),
	stringswitch_enumerator_member_name(lcm_cmdtype, panel),
	stringswitch_enumerator_member_name(lcm_cmdtype, player),
	stringswitch_enumerator_member_name(lcm_cmdtype, steamer),
	stringswitch_enumerator_member_name(lcm_cmdtype, obstacle),
	stringswitch_enumerator_member_name(lcm_cmdtype, endlevel),
} stringswitch_enumerator_name(lcm_cmdtype);

static int stringswitch_enumerator_eval_func(lcm_cmdtype) (char* str, size_t l) {
	switch(l) {
		case 3:
			if(str[0]!='n') goto main_default;
			if(str[1]!='p') goto main_default;
			if(str[2]!='c') goto main_default;
			return stringswitch_enumerator_member_name(lcm_cmdtype, npc);
		case 4:
			switch(str[0]) {
				case 'n':
					if(str[1]!='o') goto main_default;
					if(str[2]!='n') goto main_default;
					if(str[3]!='e') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdtype, none);
				case 'i':
					if(str[1]!='t') goto main_default;
					if(str[2]!='e') goto main_default;
					if(str[3]!='m') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdtype, item);
				case 't':
					switch(str[1]) {
						case 'r':
							if(str[2]!='a') goto main_default;
							if(str[3]!='p') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdtype, trap);
						case 'e':
							if(str[2]!='x') goto main_default;
							if(str[3]!='t') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdtype, text);
						default: goto main_default;
					}
				default: goto main_default;
			}
		case 5:
			switch(str[0]) {
				case 'e':
					if(str[1]!='n') goto main_default;
					if(str[2]!='e') goto main_default;
					if(str[3]!='m') goto main_default;
					if(str[4]!='y') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdtype, enemy);
				case 'p':
					switch(str[1]) {
						case 's':
							if(str[2]!='h') goto main_default;
							if(str[3]!='o') goto main_default;
							if(str[4]!='t') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdtype, pshot);
						case 'a':
							if(str[2]!='n') goto main_default;
							if(str[3]!='e') goto main_default;
							if(str[4]!='l') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdtype, panel);
						default: goto main_default;
					}
				default: goto main_default;
			}
		case 6:
			if(str[0]!='p') goto main_default;
			if(str[1]!='l') goto main_default;
			if(str[2]!='a') goto main_default;
			if(str[3]!='y') goto main_default;
			if(str[4]!='e') goto main_default;
			if(str[5]!='r') goto main_default;
			return stringswitch_enumerator_member_name(lcm_cmdtype, player);
		case 7:
			if(str[0]!='s') goto main_default;
			if(str[1]!='t') goto main_default;
			if(str[2]!='e') goto main_default;
			if(str[3]!='a') goto main_default;
			if(str[4]!='m') goto main_default;
			if(str[5]!='e') goto main_default;
			if(str[6]!='r') goto main_default;
			return stringswitch_enumerator_member_name(lcm_cmdtype, steamer);
		case 8:
			switch(str[0]) {
				case 'o':
					if(str[1]!='b') goto main_default;
					if(str[2]!='s') goto main_default;
					if(str[3]!='t') goto main_default;
					if(str[4]!='a') goto main_default;
					if(str[5]!='c') goto main_default;
					if(str[6]!='l') goto main_default;
					if(str[7]!='e') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdtype, obstacle);
				case 'e':
					if(str[1]!='n') goto main_default;
					if(str[2]!='d') goto main_default;
					if(str[3]!='l') goto main_default;
					if(str[4]!='e') goto main_default;
					if(str[5]!='v') goto main_default;
					if(str[6]!='e') goto main_default;
					if(str[7]!='l') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdtype, endlevel);
				default: goto main_default;
			}
		default:
			main_default:
			return stringswitch_enumerator_default_member_name(lcm_cmdtype);
	}
}
