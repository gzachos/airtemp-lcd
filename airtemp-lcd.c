/*
 * +-----------------------------------------------------------------------+
 * |                Copyright (C) 2018 George Z. Zachos                    |
 * +-----------------------------------------------------------------------+
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contact Information:
 * Name: George Z. Zachos
 * Email: gzzachos <at> gmail.com
 */
#define _GNU_SOURCE

#include <wiringPi.h>
#include <lcd.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>

#define SENSOR_ID             28-04146dd116ff  // Modify at your own discretion
#define SENSOR_FILEPATH(id)   "/sys/bus/w1/devices/"id"/w1_slave"
#define INVALID_TEMP_VALUE    -98.76
#define BUFFER_SIZE           128
#define DEG_CELSIUS_CHAR      0
#define LCD_ROWS              2
#define LCD_COLS              16
#define LCD_BITS              4
#define LCD_PIN_RS            11
#define LCD_PIN_E             10 // Referred as Strobe pin in wiringpi.com
#define LCD_PIN_D4            6  // Referred as D0
#define LCD_PIN_D5            5  // Referred as D1
#define LCD_PIN_D6            4  // Referred as D2
#define LCD_PIN_D7            1  // Referred as D3
#define PRECISION(temp)       (2 - (((int) (temp)) / 100))
#define SET_TEMP(val)         {pthread_mutex_lock(&temp_mutex); \
                               temp = val; \
                               pthread_mutex_unlock(&temp_mutex);}
#define GET_TEMP(sptr)        {pthread_mutex_lock(&temp_mutex); \
                               *(sptr) = temp; \
                               pthread_mutex_unlock(&temp_mutex);}
#define __STR(val)            #val
#define STR(val)              __STR(val)

/* Global declarations */
int    fd;
float  temp = INVALID_TEMP_VALUE;
char  *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
volatile sig_atomic_t exitflag = 0;
pthread_mutex_t temp_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned char degcelsius_char[8] =
{
	0b00110,
	0b01001,
	0b00110,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

/* Function definitions */
void sighandler(int signo);
void *get_temp(void *arg);
void *display_data(void *arg);


int main(void)
{
	pthread_t tid[2];

	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);

	openlog("airtemp-lcd", LOG_PID, LOG_USER);

	wiringPiSetup();

	pthread_create(tid, NULL, &get_temp, NULL);
	pthread_create(tid+1, NULL, &display_data, NULL);

	syslog(LOG_INFO, "Created threads");

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	syslog(LOG_INFO, "Program termination... (joined threads)");
	closelog();

	return EXIT_SUCCESS;
}


void sighandler(int signo)
{
	exitflag = 1;
}


void *get_temp(void *arg)
{
	FILE *sensorfile;
	char  buff[BUFFER_SIZE];
	char *temptr;

	for (; exitflag == 0;)
	{
		if (!(sensorfile = fopen(SENSOR_FILEPATH(STR(SENSOR_ID)), "r")))
		{
			syslog(LOG_INFO, "fopen: %s", strerror(errno));
			SET_TEMP(INVALID_TEMP_VALUE);
			continue;
		}
		fread(buff, BUFFER_SIZE, sizeof(char), sensorfile);
		fclose(sensorfile);

		if (strstr(buff, "YES"))
		{
			temptr = strstr(buff, "t=") + 2;
			SET_TEMP(atof(temptr)/1000);
		}
		else
		{
			syslog(LOG_INFO, "Unable to verify DS18B20 sensor data");
			SET_TEMP(INVALID_TEMP_VALUE);
		}
		usleep(200000);
	}
	return NULL;
}


void *display_data(void *arg)
{
	float curr_temp;
	time_t now;
	struct tm *tm;

	fd = lcdInit(LCD_ROWS, LCD_COLS, LCD_BITS, LCD_PIN_RS, LCD_PIN_E,
		LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7, 0, 0, 0, 0);

	if (fd == -1)
	{
		syslog(LOG_INFO, "Errors encountered during lcdInit()");
		exitflag = 1;
		return NULL;
	}

	lcdCharDef(fd, DEG_CELSIUS_CHAR, degcelsius_char);

	for (; exitflag == 0;)
	{
		now = time(NULL);
		tm = localtime(&now);
		lcdHome(fd);
		lcdClear(fd);
		lcdPrintf(fd, "%3s %02d, %02d:%02d:%02d", months[tm->tm_mon],
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		lcdPosition(fd, 0, 1);
		GET_TEMP(&curr_temp);
		lcdPrintf(fd, "Office: % 6.*f C", PRECISION(curr_temp), curr_temp);
		lcdPosition(fd, 14, 1);
		lcdPutchar(fd, DEG_CELSIUS_CHAR);
		usleep(100000);
	}
	lcdClear(fd);
	return NULL;
}

