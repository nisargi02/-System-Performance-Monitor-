/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include <signal.h>
#include <pthread.h>
#include "system.h"

/**
 * Needs:
 *   signal()
 */

static volatile int done;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void
_signal_(int signum)
{
	assert( SIGINT == signum );
	done = 1;
}

double
cpu_util(const char *s)
{
	static unsigned sum_, vector_[7];
	unsigned sum, vector[7];
	const char *p;
	double util;
	uint64_t i;

	/*
	  user
	  nice
	  system
	  idle
	  iowait
	  irq
	  softirq
	*/

	if (!(p = strstr(s, " ")) ||
	    (7 != sscanf(p,
			 "%u %u %u %u %u %u %u",
			 &vector[0],
			 &vector[1],
			 &vector[2],
			 &vector[3],
			 &vector[4],
			 &vector[5],
			 &vector[6]))) {
		return 0;
	}
	sum = 0.0;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		sum += vector[i];
	}
	util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
	sum_ = sum;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		vector_[i] = vector[i];
	}
	return util;
}

double
memory_util(FILE *file) {
	char line[256];
	unsigned long total_memory, free_memory, buffers, cached;
	double utilized_memory, utilization_precent;

	while(fgets(line, sizeof(line), file)) {
		if(sscanf(line, "MemTotal: %lu kB", &total_memory) == 1) {
			continue;
		}
		else if(sscanf(line, "MemFree: %lu kB", &free_memory) == 1) {
			continue;
		}
		else if(sscanf(line, "Buffers: %lu kB", &buffers) == 1) {
			continue;
		}
		else if(sscanf(line, "Cached: %lu kB", &cached) == 1) {
			break;
		}
	}

	utilized_memory = total_memory - free_memory - buffers - cached;
	utilization_precent = (utilized_memory / total_memory) * 100.0;

	fclose(file);
	return utilization_precent;
}

void 
network_details(FILE *file) {
	char *temp;
	char line[256];
	char vector[9][1024] = {"", "", "", "", "", "", "", "", ""};

	while(fgets(line, sizeof(line), file)) {
		if((temp = strstr(line, "eth0:"))) {
			sscanf(temp, "%*s %s %*s %*s %*s %*s %*s %*s %*s %s", vector[0], vector[8]);
			break;
		}
	}

	fclose(file);
	
	if(*vector[0] && *vector[8]) {
		printf("Bytes up/down: %s, %s\n", vector[0], vector[8]);
	}
	else {
		printf("Error in getting network details\n");
	}
}

void 
loadavg_details(FILE *file) {
	double prev_min, prev_five_min, prev_fifteen_min;

	if(fscanf(file, "%lf %lf %lf", &prev_min, &prev_five_min, &prev_fifteen_min) == 3) {
		printf("Load Average 1 min: %.2f \t 5 min: %.2f \t 15 min: %.2f\n", prev_min, prev_five_min, prev_fifteen_min);
	}
	else {
		fprintf(stderr, "Error reading load averages from file\n");
	}
}

void
process_details(FILE *file) {
	char line[1024];
	char *p;
	char vector[4][1024] = {"", "", "", ""};

	while(fgets(line, sizeof(line), file)) {
		if((p = strstr(line, "Name:"))) {
			sscanf(p, "%*s %s", vector[0]);
		}
		else if((p = strstr(line, "Pid:")) && !strcmp(vector[1], "")) {
			sscanf(p, "%*s %s", vector[1]);
		}
		else if((p = strstr(line, "VmSize:"))) {
			sscanf(p, "%*s %s", vector[2]);
		}
		else if((p = strstr(line, "Threads:"))) {
			sscanf(p, "%*s %s", vector[3]);
		}
	}

	fclose(file);
	printf("ProcessName : %s \t PID: %s \t VMem: %s \t Threads: %s\n\n", vector[0], vector[1], vector[2], vector[3]);
}


void 
*new_thread(void *args) {
	int i;
	UNUSED(args);
	for(i=0; i<5; i++) {
		us_sleep(50000);
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
	const char * const PROC_STAT = "/proc/stat";
	const char * const PROC_MEMINFO = "/proc/meminfo";
	const char * const PROC_NET = "/proc/net/dev";
	const char * const PROC_LOADAVG = "/proc/loadavg";
	const char * const PROC_PROCESS = "/proc/self/status";

	char line[1024];
	FILE *file, *file_mem, *file_net,  *file_loadavg, *file_process;
	pthread_t thread_id;

	UNUSED(argc);
	UNUSED(argv);

	if (SIG_ERR == signal(SIGINT, _signal_)) {
		TRACE("signal()");
		return -1;
	}

	if(pthread_create(&thread_id, NULL, new_thread, NULL) != 0) {
		fprintf(stderr, "Error creating new thread\n");
		return -1;
	}


	while (!done) {
		if (!(file = fopen(PROC_STAT, "r")) ||
			!(file_mem = fopen(PROC_MEMINFO, "r")) ||
			!(file_net = fopen(PROC_NET, "r")) ||
			!(file_loadavg = fopen(PROC_LOADAVG, "r")) ||
			!(file_process = fopen(PROC_PROCESS, "r"))) {
			TRACE("fopen()");
			return -1;
		}

		if (fgets(line, sizeof (line), file)) {
			pthread_mutex_lock(&mutex);
            printf("\033[H\033[J");
			printf("CPU Utilization: %5.1f%%\n", cpu_util(line));
			printf("Memory Utilization: %5.1f%%\n", memory_util(file_mem));
			network_details(file_net);
			
			loadavg_details(file_loadavg);
			process_details(file_process);
			pthread_mutex_unlock(&mutex);
			fflush(stdout);
		}
		us_sleep(500000);
		fclose(file);
	}

	pthread_join(thread_id, NULL);
	printf("\rDone!   \n");
	return 0;
}
