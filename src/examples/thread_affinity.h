#ifndef THREAD_AFFINITY_H
#define THREAD_AFFINITY_H


#include <pthread.h>


static void set_current_thread_affinity(int cpu_id_)
{
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id_, &cpu_set);

	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
}


#endif // ifndef THREAD_AFFINITY_H
