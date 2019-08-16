/************************************
  Simulation File for ARINC-653 scheduler to load schedule entries (domains) in Xen
  Author - Pavan Kumar Paluri 
  RTLABS at University of Houston - 2019
  *************************************/

/*
 Mini- Description -

 This file has to be placed in /home/rtlabuser as it takes info from pool_id and dom_uuid 
 residing in /home/rtlabuser/pool_uuid folder. It imports domain(s) uuid and poolid of the cpupool
 which is run by AAF-RRP Xen scheduler. 

 NOTE: there is no need to run this file as the shell script /home/rtlabuser/uuid_automation.sh 
 is taking care of the compilation of this program. 

 - Support to multiple domains UUID extraction will soon be added...
*/

#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xenctrl.h>
#include <uuid/uuid.h>
#define NUM_MINOR_FRAMES 1
typedef int64_t s_time_t;
#define MILLISECS(_ms)          (((s_time_t)(_ms)) * 1000000UL )
#define DOMN_RUNTIME MILLISECS(10)
#define LIBXL_CPUPOOL_POOLID_ANY 0xFFFFFFFF
#define UUID_READ 1024
char buffer[UUID_READ];
char *dom_uuid;
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

char *uu_id(FILE *filer)
{
  filer = fopen("/home/rtlabuser/pool_uuid/uuid_info.txt","r");
  if(filer == NULL)
	perror("fopen(uuid_info.txt");
  fscanf(filer,"%s",buffer);
  printf("Reading.. uuid of domain is: %s\n",buffer);
  fclose(filer);
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
    struct xen_sysctl_arinc653_schedule sched;
    xc_interface *xci = xc_interface_open(NULL, NULL, 0);
    int i;
    int result ;
 
    sched.major_frame = 0;
    sched.num_sched_entries = NUM_MINOR_FRAMES;
    for (i = 0; i < sched.num_sched_entries; ++i)
    {
        //strncpy((char *)sched.sched_entries[i].dom_handle,dom_uuid[i],sizeof(dom_uuid[i]));
        int uuid =  uuid_parse(dom_uuid, sched.sched_entries[i].dom_handle);
       printf("Size of Domain: %ld\n",sizeof(sched.sched_entries[1].dom_handle));
	printf("DomUUID return:%d\n",uuid);
	// enabling only single vcpu to run 
       sched.sched_entries[i].vcpu_id = 0;
 
        sched.sched_entries[i].runtime = 50000; 
        sched.major_frame += sched.sched_entries[i].runtime;
    }

    result = xc_sched_arinc653_schedule_set(xci,pool_id, &sched);
    printf("The cpupoolid is: %d\n",pool_id);
    printf("The result is %d\n",result);
//   sleep(50000000);
//   xc_sched_arinc653_schedule_get(xci,POOL_ID,&sched);
    printf("The number of entries is %d\n",sched.num_sched_entries);
    for(i = 0;i < sched.num_sched_entries;i++)
    {
	printf("=====%d======\n",i);
	printf("The major_frame is %llu\n",sched.major_frame);
	printf("The dom_handle is %X\n",*sched.sched_entries[i].dom_handle);
	printf("The vcpuid is %d\n",sched.sched_entries[i].vcpu_id);
	printf("The major_frame is %llu\n",sched.sched_entries[i].runtime);
	printf("==============\n");
    } 

   return 0;
}

