typedef enum {
	stringswitch_enumerator_default_member_name(lcm_cmdsubtype),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, both),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, biker),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, arrow),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, touch),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, chase),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, weapon),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, noskip),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, flydie),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, follow),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, notgrab),
	stringswitch_enumerator_member_name(lcm_cmdsubtype, project),
} stringswitch_enumerator_name(lcm_cmdsubtype);

static int stringswitch_enumerator_eval_func(lcm_cmdsubtype) (char* str, size_t l) {
	switch(l) {
		case 4:
			if(str[0]!='b') goto main_default;
			if(str[1]!='o') goto main_default;
			if(str[2]!='t') goto main_default;
			if(str[3]!='h') goto main_default;
			return stringswitch_enumerator_member_name(lcm_cmdsubtype, both);
		case 5:
			switch(str[0]) {
				case 'b':
					if(str[1]!='i') goto main_default;
					if(str[2]!='k') goto main_default;
					if(str[3]!='e') goto main_default;
					if(str[4]!='r') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, biker);
				case 'a':
					if(str[1]!='r') goto main_default;
					if(str[2]!='r') goto main_default;
					if(str[3]!='o') goto main_default;
					if(str[4]!='w') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, arrow);
				case 't':
					if(str[1]!='o') goto main_default;
					if(str[2]!='u') goto main_default;
					if(str[3]!='c') goto main_default;
					if(str[4]!='h') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, touch);
				case 'c':
					if(str[1]!='h') goto main_default;
					if(str[2]!='a') goto main_default;
					if(str[3]!='s') goto main_default;
					if(str[4]!='e') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, chase);
				default: goto main_default;
			}
		case 6:
			switch(str[0]) {
				case 'w':
					if(str[1]!='e') goto main_default;
					if(str[2]!='a') goto main_default;
					if(str[3]!='p') goto main_default;
					if(str[4]!='o') goto main_default;
					if(str[5]!='n') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, weapon);
				case 'n':
					if(str[1]!='o') goto main_default;
					if(str[2]!='s') goto main_default;
					if(str[3]!='k') goto main_default;
					if(str[4]!='i') goto main_default;
					if(str[5]!='p') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, noskip);
				case 'f':
					switch(str[1]) {
						case 'l':
							if(str[2]!='y') goto main_default;
							if(str[3]!='d') goto main_default;
							if(str[4]!='i') goto main_default;
							if(str[5]!='e') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdsubtype, flydie);
						case 'o':
							if(str[2]!='l') goto main_default;
							if(str[3]!='l') goto main_default;
							if(str[4]!='o') goto main_default;
							if(str[5]!='w') goto main_default;
							return stringswitch_enumerator_member_name(lcm_cmdsubtype, follow);
						default: goto main_default;
					}
				default: goto main_default;
			}
		case 7:
			switch(str[0]) {
				case 'n':
					if(str[1]!='o') goto main_default;
					if(str[2]!='t') goto main_default;
					if(str[3]!='g') goto main_default;
					if(str[4]!='r') goto main_default;
					if(str[5]!='a') goto main_default;
					if(str[6]!='b') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, notgrab);
				case 'p':
					if(str[1]!='r') goto main_default;
					if(str[2]!='o') goto main_default;
					if(str[3]!='j') goto main_default;
					if(str[4]!='e') goto main_default;
					if(str[5]!='c') goto main_default;
					if(str[6]!='t') goto main_default;
					return stringswitch_enumerator_member_name(lcm_cmdsubtype, project);
				default: goto main_default;
			}
		default:
			main_default:
			return stringswitch_enumerator_default_member_name(lcm_cmdsubtype);
	}
}
