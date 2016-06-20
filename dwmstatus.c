#define _DEFAULT_SOURCE
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

static Display *dpy;

char *
smprintf(char *line, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, line);
	len = vsnprintf(NULL, 0, line, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, line);
	vsnprintf(ret, len, line, fmtargs);
	va_end(fmtargs);

	return ret;
}

char *
mktimes(char *line)
{
	char buf[32];
	time_t t;
	struct tm *now;

	time(&t);
	now = localtime(&t);
	if (now == NULL) {
		perror("localtime failed\n");
		exit(1);
	}
	strftime(buf, sizeof(buf)-1, line, now);
	return smprintf("%s", buf);
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
batstat(void)
{
	short int n;
	char status;
	FILE *fp;

	if ((fp = fopen("/sys/class/power_supply/BAT0/status", "r"))) {
		status = fgetc(fp);
		fclose(fp);
		fp = fopen("/sys/class/power_supply/BAT0/capacity", "r");
		fscanf(fp, "%hd", &n);
		fclose(fp);

		if (status == 'C') /* charging */
			return smprintf("+%hd%%", n);
		else if (status == 'D') /* discharging */
			return smprintf("-%hd%%", n);
		/* nothing for full */
		return smprintf("%hd%%", n);
	}
	else
		return smprintf("AC");
}

char *
runcmd(char *cmd)
{
	FILE *fp = popen(cmd, "r");
	char buf[50];

	if (fp == NULL) {
		perror("fail to run command");
		exit(1);
	}

	fgets(buf, sizeof(buf)-1, fp); /* Get the strings from fp */
	pclose(fp);

	buf[strlen(buf)-1] = '\0'; /* Shorten the buf to it's size */
	return smprintf("%s", buf);
}

char *
getfree(char *mnt)
{
	struct statvfs buf;

	if ((statvfs(mnt, &buf)) < 0){
		perror("can't get info on disk");
		exit(1);
	}
	/* calculate the percentage of free/total */
	return smprintf("%d%%", 100 * buf.f_bfree / buf.f_blocks);
}

int
main(void)
{
	unsigned short i;
	char *tmtz, *avgs = NULL, *root = NULL, *vol = NULL, *bat = NULL;
	char *line;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (i = 0;;sleep(1), i++) {
		if (i % 10 == 0) {
			free(bat);
			bat = batstat();
		}
		if (i % 5 == 0) {
			free(avgs);
			free(root);
			free(vol);
			avgs = loadavg();
			root = getfree("/");
			vol = runcmd("amixer get PCM | grep -om1 '[0-9]*%'");
		}
		tmtz = mktimes("%a %b %d %T");

		line = smprintf("♪ %s ⚡ %s │ / %s │ %s │ %s",
				vol, bat, root, avgs, tmtz);

		XStoreName(dpy, DefaultRootWindow(dpy), line);
		XSync(dpy, False);

		free(tmtz);
		free(line);

		if (i == 60)
			i = 0;
	}

	XCloseDisplay(dpy);

	return 0;
}

