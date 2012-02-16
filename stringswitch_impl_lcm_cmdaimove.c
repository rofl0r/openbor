typedef enum {
	stringswitch_enumerator_default_member_name(lcm_cmdaimove),
	stringswitch_enumerator_member_name(lcm_cmdaimove, star),
	stringswitch_enumerator_member_name(lcm_cmdaimove, bomb),
	stringswitch_enumerator_member_name(lcm_cmdaimove, chase),
	stringswitch_enumerator_member_name(lcm_cmdaimove, avoid),
	stringswitch_enumerator_member_name(lcm_cmdaimove, biker),
	stringswitch_enumerator_member_name(lcm_cmdaimove, arrow),
	stringswitch_enumerator_member_name(lcm_cmdaimove, normal),
	stringswitch_enumerator_member_name(lcm_cmdaimove, chasex),
	stringswitch_enumerator_member_name(lcm_cmdaimove, chasez),
	stringswitch_enumerator_member_name(lcm_cmdaimove, avoidx),
	stringswitch_enumerator_member_name(lcm_cmdaimove, avoidz),
	stringswitch_enumerator_member_name(lcm_cmdaimove, wander),
	stringswitch_enumerator_member_name(lcm_cmdaimove, nomove),
} stringswitch_enumerator_name(lcm_cmdaimove);

static int stringswitch_enumerator_eval_func(lcm_cmdaimove) (char* str, size_t l) {
	switch(l) {
		case 4:
			switch(str[0]) {
				case 's':
					if(str[1]!='t') goto main_default;
					if(str[2]!='a') goto main_default;
					if(str[3]!='r') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdaimove, star);
				case 'b':
					if(str[1]!='o') goto main_default;
					if(str[2]!='m') goto main_default;
					if(str[3]!='b') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdaimove, bomb);
				default: goto main_default;
			}
		case 5:
			switch(str[0]) {
				case 'c':
					if(str[1]!='h') goto main_default;
					if(str[2]!='a') goto main_default;
					if(str[3]!='s') goto main_default;
					if(str[4]!='e') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdaimove, chase);
				case 'a':
					switch(str[1]) {
						case 'v':
							if(str[2]!='o') goto main_default;
							if(str[3]!='i') goto main_default;
							if(str[4]!='d') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdaimove, avoid);
						case 'r':
							if(str[2]!='r') goto main_default;
							if(str[3]!='o') goto main_default;
							if(str[4]!='w') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdaimove, arrow);
						default: goto main_default;
					}
				case 'b':
					if(str[1]!='i') goto main_default;
					if(str[2]!='k') goto main_default;
					if(str[3]!='e') goto main_default;
					if(str[4]!='r') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdaimove, biker);
				default: goto main_default;
			}
		case 6:
			switch(str[0]) {
				case 'n':
					if(str[1]!='o') goto main_default;
					switch(str[2]) {
						case 'r':
							if(str[3]!='m') goto main_default;
							if(str[4]!='a') goto main_default;
							if(str[5]!='l') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdaimove, normal);
						case 'm':
							if(str[3]!='o') goto main_default;
							if(str[4]!='v') goto main_default;
							if(str[5]!='e') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdaimove, nomove);
						default: goto main_default;
					}
				case 'c':
					if(str[1]!='h') goto main_default;
					if(str[2]!='a') goto main_default;
					if(str[3]!='s') goto main_default;
					if(str[4]!='e') goto main_default;
					switch(str[5]) {
						case 'x':
							return stringswitch_enumerator_member_name(lcm_cmdaimove, chasex);
						case 'z':
							return stringswitch_enumerator_member_name(lcm_cmdaimove, chasez);
						default: goto main_default;
					}
				case 'a':
					if(str[1]!='v') goto main_default;
					if(str[2]!='o') goto main_default;
					if(str[3]!='i') goto main_default;
					if(str[4]!='d') goto main_default;
					switch(str[5]) {
						case 'x':
							return stringswitch_enumerator_member_name(lcm_cmdaimove, avoidx);
						case 'z':
							return stringswitch_enumerator_member_name(lcm_cmdaimove, avoidz);
						default: goto main_default;
					}
				case 'w':
					if(str[1]!='a') goto main_default;
					if(str[2]!='n') goto main_default;
					if(str[3]!='d') goto main_default;
					if(str[4]!='e') goto main_default;
					if(str[5]!='r') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdaimove, wander);
				default: goto main_default;
			}
		default:
			main_default:
			return stringswitch_enumerator_default_member_name(lcm_cmdaimove);
	}
}
