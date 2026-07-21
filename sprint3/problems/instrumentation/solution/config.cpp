#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define BUFSIZE 255

#undef DEBUG

Config * ReadConfig (char * file)
{
	FILE * in;
	Config * retval = (Config *) malloc (sizeof(Config));

	retval->min_edgewidth = -1;
	retval->max_edgecount = 60;
	retval->ignore_refresh = 0;

	in = fopen (file, "r");

	if (in == NULL)
	{
		char * error = "Error opening configfile ('";
		char * errmsg = (char *) malloc (strlen(error) + strlen(file) + 2 + 1);
		sprintf(errmsg, "%s%s')", error, file);
		perror(errmsg);
		return retval;
	};

	char buffer[BUFSIZE];

	while (fgets(buffer, BUFSIZE, in))
	{
		char option[BUFSIZE];
		char value[BUFSIZE];
		if (!sscanf(buffer, "%s %s", &option, &value))
			continue;

#ifdef DEBUG
		fprintf(stderr, "Read option: %s (%s)\n", option, value);
#endif

		if (strcmp(option, "min_edgewidth") == 0)
		{
			sscanf(value, "%d", &retval->min_edgewidth);
		}
		else if (strcmp(option, "max_edgecount") == 0)
		{
			sscanf(value, "%d", &retval->max_edgecount);
		}
		else if (strcmp(option, "ignore_refresh") == 0)
		{
			sscanf(value, "%d", &retval->ignore_refresh);
#ifdef DEBUG
			fprintf(stderr, "Ignore_refresh is %d\n", retval->ignore_refresh);
#endif
		}
		else if ((strcmp(option, "unify") == 0)
			|| (strcmp(option, "ignore") == 0))
		{

		}
		else
		{
			fprintf(stderr, "Unknown argument: %s - ignoring.\n", option);
		}
	}

	return retval;
}
