#include "date.h"

static const char *sus_mon_str(int mon) {
	const char *s = NULL;
	switch (mon) {
		case 0:
			s = "Jan";
			break;
		case 1:
			s = "Feb";
			break;
		case 2:
			s = "Mar";
			break;
		case 3:
			s = "Apr";
			break;
		case 4:
			s = "May";
			break;
		case 5:
			s = "Jun";
			break;
		case 6:
			s = "Jul";
			break;
		case 7:
			s = "Aug";
			break;
		case 8:
			s = "Sep";
			break;
		case 9:
			s = "Oct";
			break;
		case 10:
			s = "Nov";
			break;
		case 11:
			s = "Dec";
			break;
	}
	return s;
}

static const char *sus_day_of_week_str(int dow)
{
	const char *s = NULL;
	switch (dow) {
		case 0:
			s = "Mon";
			break;
		case 1:
			s = "Tue";
			break;
		case 2:
			s = "Wed";
			break;
		case 3:
			s = "Thu";
			break;
		case 4:
			s = "Fri";
			break;
		case 5:
			s = "Sat";
			break;
		case 6:
			s = "Sun";
			break;
	}
	return s;
}

char *sus_http_gmtime(time_t _time)
{
#define DATEBUF_LEN 128
	struct tm *tm = gmtime(&_time);
	char *datebuf = malloc(DATEBUF_LEN);
	if (!datebuf) {
		sus_log_error(LEVEL_PANIC, "Failed to alloc mem for \"datebuf\"");
		exit(1);
	}

	// (Day of week), (day of month) (month) (year) (hour):(min):(sec) GMT
	snprintf(datebuf, DATEBUF_LEN, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
			sus_day_of_week_str(tm->tm_wday),
			tm->tm_mday,
			sus_mon_str(tm->tm_mon),
			1900+tm->tm_year,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);

	tm = NULL;
	return datebuf;
}
