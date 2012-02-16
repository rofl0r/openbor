typedef enum {
	stringswitch_enumerator_default_member_name(menutxt_cmd),
	stringswitch_enumerator_member_name(menutxt_cmd, renamekey),
	stringswitch_enumerator_member_name(menutxt_cmd, disablekey),
	stringswitch_enumerator_member_name(menutxt_cmd, fontmonospace),
} stringswitch_enumerator_name(menutxt_cmd);

static int stringswitch_enumerator_eval_func(menutxt_cmd) (char* str, size_t l) {
	switch(l) {
		case 9:
			if(str[0]!='r') goto main_default;
			if(str[1]!='e') goto main_default;
			if(str[2]!='n') goto main_default;
			if(str[3]!='a') goto main_default;
			if(str[4]!='m') goto main_default;
			if(str[5]!='e') goto main_default;
			if(str[6]!='k') goto main_default;
			if(str[7]!='e') goto main_default;
			if(str[8]!='y') goto main_default;
			return stringswitch_enumerator_member_name(menutxt_cmd, renamekey);
		case 10:
			if(str[0]!='d') goto main_default;
			if(str[1]!='i') goto main_default;
			if(str[2]!='s') goto main_default;
			if(str[3]!='a') goto main_default;
			if(str[4]!='b') goto main_default;
			if(str[5]!='l') goto main_default;
			if(str[6]!='e') goto main_default;
			if(str[7]!='k') goto main_default;
			if(str[8]!='e') goto main_default;
			if(str[9]!='y') goto main_default;
			return stringswitch_enumerator_member_name(menutxt_cmd, disablekey);
		case 13:
			if(str[0]!='f') goto main_default;
			if(str[1]!='o') goto main_default;
			if(str[2]!='n') goto main_default;
			if(str[3]!='t') goto main_default;
			if(str[4]!='m') goto main_default;
			if(str[5]!='o') goto main_default;
			if(str[6]!='n') goto main_default;
			if(str[7]!='o') goto main_default;
			if(str[8]!='s') goto main_default;
			if(str[9]!='p') goto main_default;
			if(str[10]!='a') goto main_default;
			if(str[11]!='c') goto main_default;
			if(str[12]!='e') goto main_default;
			return stringswitch_enumerator_member_name(menutxt_cmd, fontmonospace);
		default:
			main_default:
			return stringswitch_enumerator_default_member_name(menutxt_cmd);
	}
}
