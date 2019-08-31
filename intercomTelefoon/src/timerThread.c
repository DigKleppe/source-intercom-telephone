/*
 * timerThread.c
 *
 *  Created on: May 5, 2019
 *      Author: dig
 */

#include "telefoon.h"
#include <unistd.h>

uint32_t uptime;
void* timerThread(void* args) {
	while(1){
		sleep(1);
		uptime++;

	}
	return ((void *)0);
}

