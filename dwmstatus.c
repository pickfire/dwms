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

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs)
{
	char buf[255], *start;
	unsigned long long int receivedacc, sentacc;
	FILE *fp = fopen("/proc/net/dev", "r");
	int ret = 1;

	/* Ignore first 2 lines of file */
	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);

	while (fgets(buf, sizeof(buf), fp)) {
		if ((start = strstr(buf, "lo:")) == NULL) {
			start = strstr(buf, ":");

			sscanf(start+1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu", &receivedacc, &sentacc);
			*receivedabs += receivedacc;
			*sentabs += sentacc;
			ret = 1;
		}
	}
	fclose(fp);
	return ret;
}

#define GIGA (1024.0 * 1024 * 1024)
#define MEGA (1024.0 * 1024)
#define KILO (1024.0)

char *
cal_bytes(double b)
{
	if (b > GIGA)
		return smprintf("%.1fG", b / GIGA);
	else if (b > MEGA)
		return smprintf("%.1fM", b / MEGA);
	else if (b > KILO)
		return smprintf("%.1fk", b / KILO);
	else
		return smprintf("%.0f", b);
}

char *
netusage(unsigned long long int *oldrec, unsigned long long int *oldsent)
{
	unsigned long long int newrec, newsent;
	newrec = newsent = 0;

	if (!parse_netdev(&newrec, &newsent)) {
		fprintf(stdout, "error parsing /proc/net/dev\n");
		exit(1);
	}

	return smprintf("↓ %s ↑ %s", cal_bytes(newrec-*oldrec), cal_bytes(newsent-*oldsent));
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
	char *tmtz, *net, *avgs, *root, *vol, *bat;
	tmtz = net = avgs = root = vol = bat = NULL;
	char *line;
	static unsigned long long rec = 0, sent = 0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec, &sent);

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
		net = netusage(&rec, &sent);
		tmtz = mktimes("%a %b %d %T");

		line = smprintf("♪ %s ⚡ %s │ %s │ / %s │ %s │ %s",
				vol, bat, net, root, avgs, tmtz);

		XStoreName(dpy, DefaultRootWindow(dpy), line);
		XSync(dpy, False);

		free(net);
		free(tmtz);
		free(line);

		if (i == 60)
			i = 0;
	}

	XCloseDisplay(dpy);

	return 0;
}
