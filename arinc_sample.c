/*************************************************
 Name: ARINC653 Simulation file for  ARINC653 scheduler
 purpose: Sets the schedule entries for arinc scheduler
 To compile: gcc arinc_sample.c -lxenctrl -luuid -o global (OR) run uuid_automation.sh which 
             includes compilation of this simulation file as well.
 Author: Pavan Kumar Paluri
 Copyright @ RTLABS -2019 - University of Houston 
 *************************************************/
#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xenctrl.h>
#include <uuid/uuid.h>
#define NUM_MINOR_FRAMES 2
typedef int64_t s_time_t;
#define MILLISECS(_ms)          (((s_time_t)(_ms)) * 1000000UL )
#define DOMN_RUNTIME MILLISECS(10)
#define LIBXL_CPUPOOL_POOLID_ANY 0xFFFFFFFF
#define UUID_READ 1024
char **dom_uuid;
static int minor_frames;
//FILE *filer;
int poolid(FILE *filer)
{
  int pool_id;
  filer = fopen("/home/rtlabuser/pool_uuid/pool_id.txt","r");
   if(filer == NULL)
	perror("fopen(pool_id.txt)\n");
  fscanf(filer,"%d",&pool_id);
  printf("Reading.. cpupool id is:%d\n",pool_id);
  fclose(filer);
   return pool_id;
}

char **uu_id(FILE *filer)
{
  filer = fopen("/home/rtlabuser/pool_uuid/uuid_info.txt","r");
  if(filer == NULL)
	perror("fopen(uuid_info.txt");
  int i=0;
  char **buffer = malloc(128*sizeof(char *));
  int k;
   for(k=0;k<128;k++)
	buffer[k] = malloc(128*sizeof(char));
  
 while(fscanf(filer,"%s",buffer[i])==1)
 {
  printf("Reading.. uuid of domain is: %s\n",buffer[i]);
  i++;
 }
 printf("Number of entries are:%d\n",i);
  fclose(filer);
  minor_frames = i;
  return buffer;
}

//char dom_uuid[40]="49f86d53-3298-43a6-b8e6-a5aec2d4d3c2"; 
int main()
{
    FILE *filer;
   FILE *filer1;
   int pool_id;
    pool_id = poolid(filer1);
    dom_uuid = uu_id(filer);
    struct xen_sysctl_scheduler_op ops;
    struct xen_sysctl_aaf_schedule sched;
    xc_interface *xci = xc_interface_open(NULL, NULL, 0);
    int i;
    int result ;
 
    sched.hyperperiod = 0;
    sched.num_schedule_entries = NUM_MINOR_FRAMES;
    for (i = 0; i < sched.num_schedule_entries; ++i)
    {
        //strncpy((char *)sched.sched_entries[i].dom_handle,dom_uuid[i],sizeof(dom_uuid[i]));
        int uuid =  uuid_parse(dom_uuid[i], sched.schedule[i].dom_handle);
       printf("Size of Domain: %ld\n",sizeof(sched.schedule[1].dom_handle));
	printf("DomUUID return:%d\n",uuid);
	// enabling only single vcpu to run 
       sched.schedule[i].vcpu_id = 0;
 
        sched.schedule[i].wcet = 60000; 
        sched.hyperperiod += sched.schedule[i].wcet;
    }
    
    result = xc_sched_aaf_schedule_set(xci,pool_id, &sched);
    printf("The cpupoolid is: %d\n",pool_id);
    printf("The result is %d\n",result);
//   sleep(50000000);
//    xc_sched_arinc653_schedule_get(xci,pool_id,&sched);
    printf("The number of entries is %d\n",sched.num_schedule_entries);
    for(i = 0;i < sched.num_schedule_entries;i++)
    {
	printf("=====%d======\n",i);
	printf("The major_frame is %llu\n",sched.hyperperiod);
	printf("The dom_handle is %X\n",*sched.schedule[i].dom_handle);
	printf("The vcpuid is %d\n",sched.schedule[i].vcpu_id);
	printf("The minor_frame runtime is %llu\n",sched.schedule[i].wcet);
	printf("==============\n");
    } 

   return 0;
}

/*
 int poolid(FILE *filer)
{
    int pool_id;
    filer = fopen("/home/rtlabuser/pool_uuid/pool_id.txt","r");
    if(filer == NULL)
        perror("fopen(pool_id.txt)\n");
    fscanf(filer,"%d",&pool_id);
    printf("Reading.. cpupool id is:%d\n",pool_id);
    fclose(filer);
    return pool_id;
}

char **uu_id(FILE *filer)
{
    filer = fopen("/home/rtlabuser/pool_uuid/uuid_info.txt","r");
    if(filer == NULL)
        perror("fopen(uuid_info.txt");
    int i=0;
    char **buffer = malloc(128*sizeof(char *));
    int k;
    for(k=0;k<128;k++)
        buffer[k] = malloc(128*sizeof(char));

    while(fscanf(filer,"%s",buffer[i])==1)
    {
        printf("Reading.. uuid of domain is: %s\n",buffer[i]);
        i++;
    }
    printf("Number of entries are:%d\n",i);
    fclose(filer);
    minor_frames = i;
    return buffer;
}

 */

