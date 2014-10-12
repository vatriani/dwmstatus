/*
 * dwmstatus - main.cpp
 * Copyright © 2014 - Niels Neumann  <vatriani.nn@googlemail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "build_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <sys/sysinfo.h>

#define THRESHOLD 5
#define TIMEOUT   40
#define SUSPEND   { BOX_SUSPEND, NULL }     /* BOX_SUSPEND gets configured in Makefile */

#define LABUF     14
#define DTBUF     22
#define LNKBUF    8
#define STR       100
#define FREQ      8

/* Available statuses 
 *
 *  Charging
 *  Discharging
 *  Unknown
 *  Full
 */
typedef enum {
  C, D, U, F
} status_t;

#ifdef SUSPEND
static void spawn(const char **params)  __attribute__ ((unused));
#endif
static void set_status(char *str);
static void open_display(void)          __attribute__ ((unused));
static void close_display();
static void get_datetime(char *buf);
static float cpuavg(void) __attribute__ ((unused));
static float percentageUsageRam(void)	__attribute__ ((unused));
static status_t get_status();
static int read_int(const char *path);
static void read_str(const char *path, char *buf, size_t sz);

static Display *dpy;
static unsigned long u_time_start;
static unsigned long time_start;

float
cpuavg(void)
{
	unsigned long long user, nice, system, idle;
	unsigned long time_end, u_time_end;
	double ret = 0.0;	

	FILE *data;
	data = fopen(LA_PATH,"r");
	fscanf(data,"%*s %llu %llu %llu %llu", &user, &nice, &system, &idle);
	fclose(data);

	u_time_end = user + nice + system;
	time_end = user + nice + system + idle;	
	
	if(time_start)
	{
		ret = ((double)(u_time_end - u_time_start))/((double)(time_end - time_start))*100.0;		
	}
	u_time_start = u_time_end;
	time_start = time_end;
	return ret;
}


float
percentageUsageRam(void)
{
	struct sysinfo info;
	
	if(!sysinfo(&info))
	{
	 	return (double)info.bufferram / (double)info.totalram * 100.0;
	}
	return 0.0;
}


int
main(void)
{
  int   timer = 0;
  float bat;                /* battery status */
  char  lnk[STR] = { 0 };   /* wifi link      */
  char  dt[STR] = { 0 };    /* date/time      */
  char  stat[STR] = { 0 };  /* full string    */
  float freq;
  status_t st;              /* battery status */
  char  status[] = { '+', '-', '?', '=' };  /* should be the same order as the enum above (C, D, U, F) */

  time_start = 0;
#ifndef DEBUG
  open_display();
#endif

  while (!sleep(1)) {
    read_str(LNK_PATH, lnk, LNKBUF);        /* link status */
    freq = (float)read_int(FREQ_STAT) / 1000.0/1000.0;
    get_datetime(dt);                       /* date/time */
    bat = ((float)read_int(BAT_NOW) /
           read_int(BAT_FULL)) * 100.0f;    /* battery */
    st = get_status();                      /* battery status (charging/discharging/full/etc) */

    if (st == D && bat < THRESHOLD) {
#ifdef SUSPEND
      snprintf(stat, STR, "LOW BATTERY: remaining %0.1f%% suspending after %d ", bat,TIMEOUT - timer);
#else
      snprintf(stat, STR, "!!! LOW BATTERY !!! remaining %0.1f%%", bat);
#endif
      set_status(stat);
#ifdef SUSPEND 
      if (timer >= TIMEOUT) {
#ifndef DEBUG
        spawn((const char*[])SUSPEND);
#else
        puts("sleeping");
#endif
        timer = 0;
      } else
        timer++;
#endif
    } else {
     snprintf(stat, STR, "cpu %0.1f%% %0.1f GHz | ram %1.0f% | wifi %s | %c%1.0f%% | %s", cpuavg(), freq, percentageUsageRam(), lnk, status[st], (bat > 100.0) ? 100.0 : bat, dt);
      set_status(stat);
      timer = 0;  /* reseting the standby timer */
    }
  }

#ifndef DEBUG
  close_display();
#endif
  return 0;
}


#ifdef SUSPEND
static void
spawn(const char **params) {
  if (fork() == 0) {
    setsid();
    execv(params[0], (char**)params);
    exit(0);
  }
}
#endif


static void
set_status(char *str)
{
#ifndef DEBUG
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
#else
  puts(str);
#endif
}

static void
open_display(void)
{
  if (!(dpy = XOpenDisplay(NULL))) 
    exit(1);
  signal(SIGINT, close_display);
  signal(SIGTERM, close_display);
}

static void
close_display()
{
  XCloseDisplay(dpy);
  exit(0);
}

static void
get_datetime(char *buf)
{
  time_t rawtime;
  time(&rawtime);
  struct tm* timestruct = localtime(&rawtime);
  strftime(buf, DTBUF, "%a %d.%m.%Y %H:%M", (const struct tm*)timestruct);
}

static status_t
get_status()
{
  FILE *bs;
  char st;

  if ((bs = fopen(BAT_STAT, "r")) == NULL)
    return U;

  st = fgetc(bs);
  fclose(bs);

  switch(st) {
    case 'C': return C;     /* Charging */
    case 'D': return D;     /* Discharging */
    case 'F': return F;     /* Full */
    default : return U;     /* Unknown */
  }
}

static int
read_int(const char *path)
{
  int i = 0;
  FILE *fh;

  if (!(fh = fopen(path, "r")))
    return -1;

  fscanf(fh, "%d", &i);
  fclose(fh);
  return i;
}

static void
read_str(const char *path, char *buf, size_t sz)
{
  FILE *fh;
  char ch = 0;
  int idx = 0;

  if (!(fh = fopen(path, "r"))) return;

  while ((ch = fgetc(fh)) != EOF && ch != '\0' && ch != '\n' && idx < sz) {
    buf[idx++] = ch;
  }

  buf[idx] = '\0';
  fclose(fh);
}

