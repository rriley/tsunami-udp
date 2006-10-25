#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "parse_evn_filename.h"

void add_aux_entry(struct evn_filename *ef, char *auxentry) {
	char **p;
	ef->nr_auxinfo++;
	p = realloc(ef->auxinfo, sizeof(char*) * ef->nr_auxinfo);
	assert(p);
	ef->auxinfo = p;
	ef->auxinfo[ef->nr_auxinfo - 1] = auxentry;
}

	
double year_to_utc(int year) {
	struct tm tm;
	/*	memset(&tm, 0, sizeof(struct tm)); */
	tm.tm_year = year - 1900;
        tm.tm_mon = 0;
        tm.tm_mday = 1;
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = 0;
	return (double)mktime(&tm);
}

int get_current_year() {
	time_t t;
	struct tm *tm;
	time(&t);
	tm = gmtime(&t);
	return tm->tm_year + 1900;
}

double day_to_utc(int day) { return (day-1)*24*60*60; }
double hour_to_utc(int hour) { return hour*60*60; }
double minute_to_utc(int minute) { return minute*60; }

int parse_time(char *str, double *retval) {
	int yyyy, mm, hh, yday, sec;
	int consumed;

	if (sscanf(str, "%4dy%dd%n",
		   &yyyy,
		   &yday,
		   &consumed) == 2 && consumed == strlen(str)) {
		*retval = year_to_utc(yyyy) + day_to_utc(yday);
		return 0;
	}
	if (sscanf(str, "%4d%d%n",
			  &yyyy,
			  &yday,
			  &consumed) == 2 && consumed == strlen(str)) {
		*retval = year_to_utc(yyyy) + day_to_utc(yday);
		return 0;
	}
	if (sscanf(str, "%dd%dh%dm%ds%n",
			  &yday,
			  &hh,
			  &mm,
			  &sec,
			  &consumed) == 4 && consumed == strlen(str)) {
		*retval =  year_to_utc(get_current_year()) + day_to_utc(yday)
			+ hour_to_utc(hh) + minute_to_utc(mm) + sec;
                printf("%e %e %e %e %e\n", year_to_utc(get_current_year())
                    ,day_to_utc(yday), hour_to_utc(hh), minute_to_utc(mm),
                    (double)sec);

		return 0;
	}
	return 1;
}


char *get_aux_entry(char *key, char **auxinfo, int nr_auxinfo) {
	int i;
	for (i=0; i<nr_auxinfo; i++) {
		
		if(strlen(auxinfo[i]) > strlen(key)
		   &&!strncmp(auxinfo[i], key, strlen(key))
		   && auxinfo[i][strlen(key)] == '=') {
			return strdup(auxinfo[i]+strlen(key)+1);
		}
	}
	return NULL;
}
			
/*
 * Return one token from underscore delimited string.
 * @param pointer to pointer to part of the string that is currently
 * parsed. Function updates the pointer so that it points to the next
 * element.
 * @return element allocated with malloc(), caller must free() it.
 */
char *get_token(char **str) {
	char *p, *retval;
	if (!*str || !*str[0])
		return NULL;
	p = strchr(*str, (int)'_');
	if (!p) {
		p = strchr(*str, (int)'\0');
		assert(p);
		retval = strdup(*str);
		*str = p;
	} else {
		retval = strndup(*str, p - *str);
		*str = p + 1;
	}
	return retval;
}
/* Parse EVN filename
 * @param pointer to filename
 * @return malloc()'d struct that contains malloc()'d filename elements.
 */
struct evn_filename *parse_evn_filename(char *filename) {
	struct evn_filename *ef;
	char *parsebuf, *parseptr;
	
	ef = calloc(sizeof(struct evn_filename), 1);
	assert(ef);

	parseptr = parsebuf = strdup(filename);
	assert(parsebuf);

	/* Extract filetype from parsebuf. Overwrite dot with zero so
	   that filetype does not complicate parsing of other parts.*/
	{
		char *dot, *filetype;
		dot = strrchr(parseptr, (int)'.');
		assert(dot);
		filetype = dot + 1;
		ef->file_type = get_token(&filetype);
		assert(ef->file_type);
		assert(strlen(ef->file_type) >= 2);
		*dot = 0;
	}

	ef->exp_name = get_token(&parseptr);
	assert(ef->exp_name);
	assert(strlen(ef->exp_name) <= 6);

	ef->station_code = get_token(&parseptr);
	assert(ef->station_code);
	assert(strlen(ef->station_code) >= 2);

	ef->scan_name = get_token(&parseptr);
	assert(ef->scan_name);
	assert(strlen(ef->scan_name) <= 16);

	/* All mandatory elements read. */

	ef->data_start_time_ascii = get_token(&parseptr);
	if (ef->data_start_time_ascii) {
		assert(strlen(ef->data_start_time_ascii) >= 2);
		if (parse_time(ef->data_start_time_ascii, &ef->data_start_time)) {
			/* Does not look like date, must be auxentry instead. */
			add_aux_entry(ef, ef->data_start_time_ascii);
			ef->data_start_time_ascii = NULL;
		}
	}

	{
		char *auxentry;
		while ((auxentry = get_token(&parseptr)) != NULL)
			add_aux_entry(ef, auxentry);
	}

	free(parsebuf);
	return ef;
}

#ifdef UNIT_TEST
int main(int argc, char **argv) {
	struct evn_filename *ef;
	int i;
	if (argc < 2) {
		printf("parsing gre53_ef_scan035_154d12h43m10s.vsi\n");
		ef = parse_evn_filename("gre53_ef_scan035_154d12h43m10s.vsi");
	} else {
		ef = parse_evn_filename(argv[1]);
	}
	printf("ef->exp_name = %s\n", ef->exp_name);
	printf("ef->station_code = %s\n", ef->station_code);
	printf("ef->scan_name = %s\n", ef->scan_name);
	printf("ef->data_start_time_ascii = %s\n", ef->data_start_time_ascii);
	printf("ef->data_start_time = %f\n", ef->data_start_time);
	for (i=0; i<ef->nr_auxinfo; i++)
		printf("ef->auxinfo[%d] = %s\n", i, ef->auxinfo[i]);
	printf("ef->file_type = %s\n", ef->file_type);
	return 0;
}
#endif
