#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

char *
mktimes(char *fmt)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	float avgs[1];
	FILE *fp = fopen("/proc/loadavg", "r");

	if (fp == NULL) {
		perror("fail to open /proc/loadavg");
		exit(1);
	}

	fscanf(fp, "%f", &avgs[0]);
	fclose(fp);

	return smprintf("%.2f", avgs[0]);
}

char *
getvol(void)
{
	FILE* pipe = popen("amixer get PCM | grep -o '[0-9]*%'", "r");
	if (pipe) {
		char buffer[4];
		while(fgets(buffer, sizeof(buffer), pipe) != NULL) {
			return smprintf("%s", buffer);
		}

		pclose(pipe);
		/* buffer[strlen(buffer)-1] = '\0'; */
	}
}

int
main(void)
{
	char *status;
	char *vol;
	char *avgs;
	char *tmtz;
	unsigned int i;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (i = 0;;sleep(1), i++) {
		if (i % 5 == 0) {
			free(avgs);
			free(vol);
			avgs = loadavg();
			vol = getvol();
		}
		tmtz = mktimes("%a %d %b %T");

		status = smprintf("♪:%s │ %s │ %s",
				vol, avgs, tmtz);
		setstatus(status);
		free(tmtz);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

