#ifndef PARSE_EVN_FILENAME
#define PARSE_EVN_FILENAME

struct evn_filename {
	char *exp_name;
	char *station_code;
	char *scan_name;
	char *data_start_time_ascii;
	double data_start_time;
	char **auxinfo; /* pointer to (array of pointers to aux info elements) */
	int nr_auxinfo;
	char *file_type;
};

struct evn_filename *parse_evn_filename(char *filename);

char *get_aux_entry(char *key, char **auxinfo, int nr_auxinfo);

#endif
