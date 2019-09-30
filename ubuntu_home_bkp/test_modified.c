#include<stdio.h>
#include<stdlib.h>
#include<sys/time.h>
#include<inttypes.h>
#define MILLION  1000000000.0;
#define THOUSAND 1000.0;
#define MILLION_SEC 1000000

//keep the CPU run for wcet milliseconds.
void execute(double wcet)
{
	struct timeval start;
	gettimeofday(&start, 0);
	while(1)
	{
		struct timeval end;
        gettimeofday(&end,0);
        // Microsec precision
        long elapsed = (end.tv_sec - start.tv_sec)*MILLION_SEC + (end.tv_usec - start.tv_usec);
	//printf("Elapsed time:%lf \n",elapsed);
		if(elapsed >=wcet)
			break;
	}
}



int main(int argc, char *argv[])
{
	if(argc<3)
	{
		printf("Usage: The first parameter is the density and the second parameter is the WCET (ms).\n");
		return 0;
	}
	double density = atof(argv[1]);
	double wcet = atof(argv[2]);
	int phase = 1;
	double ddl = wcet / density;
	struct timeval start_time;
	//clock_gettime(CLOCK_REALTIME, &start_time);
     gettimeofday(&start_time, 0);
	while(1)
	{
		execute(wcet);
		struct timeval end_time;
		//clock_gettime(CLOCK_REALTIME, &end_time);
        gettimeofday(&end_time, 0);
		//double interval = (end_time.tv_sec - start_time.tv_sec)*THOUSAND;
		//interval += (end_time.tv_nsec - start_time.tv_nsec)/MILLION;
        long elapsed_interval = (end_time.tv_sec - start_time.tv_sec)*MILLION_SEC + (end_time.tv_usec - start_time.tv_usec);
		if(elapsed_interval > ddl*phase)
			printf("Job %d fails to meet the deadline %lf while it ends at %ld.\n", phase, ddl*phase, elapsed_interval);
		else
			printf("Job %d meets its deadline %lf and it ends at %ld.\n", phase, ddl*phase, elapsed_interval);
		phase++;
		fflush(stdout);
	}


}
