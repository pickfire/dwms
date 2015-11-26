#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

#include <alsa/asoundlib.h>
#include <alsa/control.h>

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

char *getfree(char *mnt){
	struct statvfs buf;

	if ( (statvfs(mnt, &buf)) < 0){
		perror("can't get info on disk");
		exit(1);
	}
	/* calculate the percentage of free/total */
	return smprintf("%d%%", 100 * buf.f_bfree / buf.f_blocks);
}

/*
int
get_vol(void)
{
    int vol;
    snd_hctl_t *hctl;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;

// To find card and subdevice: /proc/asound/, aplay -L, amixer controls
    snd_hctl_open(&hctl, "hw:0", 0);
    snd_hctl_load(hctl);

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

// amixer controls
    snd_ctl_elem_id_set_name(id, "Master Playback Volume");

    snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);

    snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);

    snd_hctl_elem_read(elem, control);
    vol = (int)snd_ctl_elem_value_get_integer(control,0);

    snd_hctl_close(hctl);
    return vol;
}
*/

int
main(void)
{
	unsigned int i;
	char *tmtz, *avgs, *root, *vol = NULL;
	char *status;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (i = 0;;sleep(1), i++) {
		if (i % 5 == 0) {
			free(avgs);
			free(root);
			free(vol);
			avgs = loadavg();
			root = getfree("/");
			vol = runcmd("amixer get PCM | grep -o '[0-9]*%'");
		}
		tmtz = mktimes("%a %d %b %T");

		status = smprintf("♪:%s │ / %s │ %s │ %s",
				vol, root, avgs, tmtz);
		setstatus(status);
		free(tmtz);
		free(status);

		if (i == 600) {
			i = 1;
		}
	}

	XCloseDisplay(dpy);

	return 0;
}

