/* Partition Single (ps) Algorithm Xen Implementation
 * Pavan Kumar Paluri
 * Copyright 2019 - RTLAB UNIVERSITY OF HOUSTON */

#include <xen/config.h>
#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/sched-if.h>
#include <xen/timer.h>
#include <xen/softirq.h>
#include <xen/time.h>
#include <xen/errno.h>
#include <xen/list.h>
#include <xen/guest_access.h>
#include <public/sysctl.h>

// Have a default time slice for Domain0's vcpus to run
#define DOM0_TS MILLISECS(10)
// get the idle vcpu for a given cpu - to run idle time slices in case no domain runs 
#define IDLETASK(cpu)   (idle_vcpu[cpu])


// typecast Xen's vcpu to our vcpu
#define PSVCPU(vc)      ((struct ps_vcpu_t *)(vc)->sched_priv)

// Given scheduler ops pointer, return ps scheduler's  private struct 
#define SCHED_PRIV(s)   ((struct ps_priv_t *)((s)->sched_data))

/********************
* Scheduler Customization Structures
*********************/

// vcpu struct for ps
struct ps_vcpu_t 
{
  struct vcpu* vc;
  // bool variable to see if vcpu is awake or asleep
  bool_t awake;
  // Linked list of vcpus to be maintained in global scheduler structure
  struct list_head list_elem;
};

// schedulable entry strucutre for ps scheduler
struct sched_entry_t
{

  // dom_handle handles the UUID for the domain that this schedulable entry refers to
  xen_domain_handle_t dom_handle;
  // vcpu_id refers to the vcpu number for vcpu that this sched_entry refers to..
  int vcpu_id;
 // wcet refers to the computation time that the vcpu for this sched_entry runs for
 //  per hyperperiod
  s_time_t wcet; //getAAFUp()
  struct vcpu* vc;
};

// Structure defines data that is global to an instance of the scheduler
struct ps_priv_t
{
   spinlock_t lock;
  // Private scheduler's struct holds all the schedulable entries
  // PS_MAX_DOMAINS_PER_SCHEDULE :: defined in sysctl.h
  struct sched_entry_t schedule[PS_MAX_DOMAINS_PER_SCHEDULE];
  /***
   * Number of valid schedulable entries in ps_schedule_table
   * Num of schedulable_entires not necessarily be equal to number of domains
   * since, each domain can appear multiple times within the schedule
   ***/
  int num_schedule_entries;
  // hyperperiod of ps scheduler
  s_time_t hyperperiod; // getAAFDown()
  // the time the next hyperperiod begins
  s_time_t next_hyperperiod;

  // Iterate through list of vcpu structures provided by xen 
   struct list_head vcpu_list;
};


/* **********************************
  * Helper Functions
  * Compare domain handles of 2 xen_domain_handle_t's
  **********************************
  * >0 : h1 > h2
  * =0 : h1 == h2
  * <0 : h1 < h2
*/

static int dom_handle_cmp(const xen_domain_handle_t h1,
			  const xen_domain_handle_t h2)
{
	return memcmp(h1, h2, sizeof(xen_domain_handle_t));
}

static int dom_comp(s_time_t wcet1, s_time_t wcet2)
{
  return wcet1 > wcet2 ? 0 : 1;
}

/* Find a vcpu based on its vcpu_id and the domain handle linked to it */
static struct vcpu* find_vcpu(const struct scheduler *ops,
			     xen_domain_handle_t h,int vcpu_id)
{
  struct ps_vcpu_t *ps_vcpu;
  // loop thru the vcpu list to find the specified VCPU 
  list_for_each_entry(ps_vcpu, &SCHED_PRIV(ops)->vcpu_list, list_elem)
    if(dom_handle_cmp(ps_vcpu->vc->domain->handle,h)==0
	&&(vcpu_id == ps_vcpu->vc->vcpu_id) )
	return ps_vcpu->vc;

  // if no such vcpu found:: Exit
   return NULL;
}


// Update the schedule entries with runnable vcpus that are validated by Xen
static void update_schedule_vcpus(const struct scheduler *ops)
{
  unsigned int i;
  int num_entries = SCHED_PRIV(ops)->num_schedule_entries;
  for(i=0; i<num_entries; i++)
  {
     // Update the schedule entries with valid vcpus that are recognized and valid by Xen
	SCHED_PRIV(ops)->schedule[i].vc =
		find_vcpu(ops,SCHED_PRIV(ops)->schedule[i].dom_handle,
			SCHED_PRIV(ops)->schedule[i].vcpu_id);
  
   }
}


// Set Boundary Conditions
// Put a New PS schedule
// This function is called by adjust_global() to put in a new PS schedule
static int ps_sched_set(const struct scheduler *ops, struct xen_sysctl_aaf_schedule *schedule)
{
  printk("Entering schedule_set AAF\n"); 
  struct ps_priv_t *sched_priv = SCHED_PRIV(ops);
 unsigned int i;
  unsigned long flags;
  s_time_t wcet_total =0; //sum of all computation times of domains
  // Assign a constant (never changing) domain handle for domain-0
 // const xen_domain_handle_t dom0_handle = {0};
  // Error handling of hyperperiod and #schedule_entries
  int rc = -EINVAL;
  spin_lock_irqsave(&sched_priv->lock,flags);

  if(schedule->hyperperiod <=0)
   {
      printk("Allocation Failure\n");
	goto dump;
   }
  if(schedule->num_schedule_entries <1 )
   {
      printk("Allocation Failure\n");
 	goto dump;
   }
  // compute total wcet
  // C-99 not supported, inits cannot be done inside for-loop
 for(i=0; i < schedule->num_schedule_entries; i++)
   {
	if(schedule->schedule[i].wcet <= 0)
        {
            printk("WCET is less than 0!!\n");
		goto dump;
        }
	
}


   // After all error-checking is done, now its time to copy schedule to new place
    sched_priv->num_schedule_entries = schedule->num_schedule_entries;
    sched_priv->hyperperiod = schedule->hyperperiod;
   for(i=0 ;i< schedule->num_schedule_entries; i++)
    {
	//COPY EVERYTHING (dom_handle, vcpu_ids and wcet's)
        // memcpy(void *to, const void* from, size_t len)
        printk("Memcpy about to happen ....\n"); 
       memcpy(sched_priv->schedule[i].dom_handle, schedule->schedule[i].dom_handle, sizeof(schedule->schedule[i].dom_handle));
        sched_priv->schedule[i].vcpu_id = schedule->schedule[i].vcpu_id;
        sched_priv->schedule[i].wcet = schedule->schedule[i].wcet;
       
    }
    printk("Memcpy and import of other params of sched_entries done\n");
    update_schedule_vcpus(ops);

     sched_priv->next_hyperperiod = NOW();
      rc =0;
   dump:
        printk("Control going to Dump Failure...\n");
        spin_unlock_irqrestore(&sched_priv->lock,flags);
	return rc;
}

// Get PS schedule
static int ps_sched_get(const struct scheduler *ops, struct xen_sysctl_aaf_schedule *schedule)
{
  struct ps_priv_t *sched_priv = SCHED_PRIV(ops);
  unsigned int i;
  unsigned long flags;
  spin_lock_irqsave(&sched_priv->lock, flags);
 schedule->num_schedule_entries = sched_priv->num_schedule_entries;
 schedule->hyperperiod = sched_priv->hyperperiod;

 for(i=0; i< sched_priv->num_schedule_entries; i++)
 {
	 memcpy(schedule->schedule[i].dom_handle,
               sched_priv->schedule[i].dom_handle,
               sizeof(sched_priv->schedule[i].dom_handle));
        schedule->schedule[i].vcpu_id = sched_priv->schedule[i].vcpu_id;
        schedule->schedule[i].wcet = sched_priv->schedule[i].wcet;
 }
  spin_unlock_irqrestore(&sched_priv->lock, flags);
 return 0;
}


/* Function to compare and return a sorted list of sched_entries based on AAFUP i.e., wcet values */
struct sched_entry_t *dom_wcet_comp(const struct scheduler *ops)
{
 struct ps_priv_t *sched_priv = SCHED_PRIV(ops);
 int i,j;
 for(i=0; i< sched_priv->num_schedule_entries; i++)
 {
	for(j=i+1; j< sched_priv->num_schedule_entries; j++)
	{
	   int k;
	   k = dom_comp(sched_priv->schedule[i].wcet, sched_priv->schedule[j].wcet);
	   if (k != 0)
	    {
             // swap the contents to sort it in ascending order of wcet(s)
              int temp;
               temp = sched_priv->schedule[i].wcet;
		sched_priv->schedule[i].wcet = sched_priv->schedule[j].wcet;
		sched_priv->schedule[j].wcet = temp;
	    }
	}
  }
  return sched_priv->schedule;
}

/********************************************
 * SCHEDULER CALLBACK FUNCTIONS 
 ********************************************/

static int aafsched_init(struct scheduler *ops)
{
	struct ps_priv_t *sched_priv;
	sched_priv = xzalloc(struct ps_priv_t);
	if(sched_priv == NULL)
		return -ENOMEM;

	ops->sched_data = sched_priv;
	// Init all the params of ps_priv_t
	sched_priv->next_hyperperiod = 0;
	// NOTE: DO NOT INIT OTHER PARAMS (hyperperiod, number of schedule entries) as they 
	// are being supplied through public/sysctl.h
	spin_lock_init(&sched_priv->lock);
        INIT_LIST_HEAD(&sched_priv->vcpu_list);
	return 0;
}


static void aafsched_deinit(struct scheduler *ops)
{
	xfree(SCHED_PRIV(ops));
	ops->sched_data = NULL;
}


static void* aafsched_alloc_vdata(const struct scheduler *ops, struct vcpu *vc, void *dd)
{
	struct ps_priv_t *sched_priv = SCHED_PRIV(ops);
	struct ps_vcpu_t *avc;
	unsigned int entry;
       unsigned long flags;
	// This version is work under progress, hence not guarded by any locks
	// Allocate memory for PS AAF scheduler associated with given VCPU
	avc = xmalloc(struct ps_vcpu_t);
	if(avc == NULL)
	    return NULL;
	spin_lock_irqsave(&sched_priv->lock, flags);
	// Now add every vcpu of Dom0 to the schedule,as long as there are enough slots
	if(vc->domain->domain_id == 0)
	{
	  entry = sched_priv->num_schedule_entries;
	  if(entry < PS_MAX_DOMAINS_PER_SCHEDULE)
	  {
	   sched_priv->schedule[entry].dom_handle[0] = '\0';
	   sched_priv->schedule[entry].vcpu_id = vc->vcpu_id;
	   sched_priv->schedule[entry].wcet = DOM0_TS;
	   sched_priv->schedule[entry].vc = vc;
           // Add up the same for major frame as well, so that it keeps running round-robin for Dom0
	   sched_priv->hyperperiod += DOM0_TS;
	   // Pre-Increment the count after every update
	    ++(sched_priv->num_schedule_entries);
	  }
       }

	// Awake/Asleep feature yet to be added...
        // VCPU will be in asleep state if not specifically woken up and this will give an error if no vcpu in dom0 is brought
        // to awake state.
	avc->vc = vc;
        avc->awake = 0;
	if(!is_idle_vcpu(vc))
	   list_add(&avc->list_elem, &SCHED_PRIV(ops)->vcpu_list);
	// After adding the vcpu to the list, update the vcpus
	update_schedule_vcpus(ops);
        spin_unlock_irqrestore(&sched_priv->lock, flags);
       return avc;
}

static void aaf_free_vdata(const struct scheduler *ops, void *priv)
{
  struct ps_vcpu_t *pvcpu = priv;
  if(pvcpu == NULL)
	return;
 if(!is_idle_vcpu(pvcpu->vc))
	list_del(&pvcpu->list_elem);
 xfree(pvcpu);
 update_schedule_vcpus(ops);
}


// AAF callback functions for sleeping and awaking scheduler's vcpus
static void aaf_vcpu_sleep(const struct scheduler *ops, struct vcpu *vc)
{
 if(PSVCPU(vc) != NULL)
   PSVCPU(vc)->awake =0;
  /* If by any chance, the VCPU being put to sleep is the same as one that is currently running,
   * raise a SOft interrupt request to let scheduler know to switch domains */
  if(per_cpu(schedule_data, vc->processor).curr == vc)
	cpu_raise_softirq(vc->processor, SCHEDULE_SOFTIRQ);
}

// Waking up the scheduler
static void aaf_vcpu_wake(const struct scheduler *ops, struct vcpu *vc)
{
 if(PSVCPU(vc) != NULL)
   PSVCPU(vc)->awake =1;
 
  cpu_raise_softirq(vc->processor, SCHEDULE_SOFTIRQ);
}

// temp do_schedule
/*
static struct task_slice aaf_schedule(const struct scheduler *ops, s_time_t now, bool_t tasklet_work_scheduled)
{
  struct task_slice ret;
  struct list_head *pos;
 struct ps_priv_t *sched_priv = SCHED_PRIV(ops);
 struct ps_vcpu_t *vc = NULL;
  ret.time = MILLISECS(10);
 const int cpu = smp_processor_id();
 if(vcpu_runnable(vc->vc))
  {
	ret.task = vc->vc;
	break;
  }
return ret;
}
*/

//#include "xc_private.h"
/*
int
xc_sched_arinc653_schedule_set(
    xc_interface *xch,
    uint32_t cpupool_id,
    struct xen_sysctl_arinc653_schedule *schedule)
{
    int rc;
    DECLARE_SYSCTL;
    DECLARE_HYPERCALL_BOUNCE(
        schedule,
        sizeof(*schedule),
        XC_HYPERCALL_BUFFER_BOUNCE_IN);

    if ( xc_hypercall_bounce_pre(xch, schedule) )
        return -1;

    sysctl.cmd = XEN_SYSCTL_scheduler_op;
    sysctl.u.scheduler_op.cpupool_id = cpupool_id;
    sysctl.u.scheduler_op.sched_id = XEN_SCHEDULER_AAF;
    sysctl.u.scheduler_op.cmd = XEN_SYSCTL_SCHEDOP_putinfo;
    set_xen_guest_handle(sysctl.u.scheduler_op.u.sched_aaf.schedule,
            schedule);

    rc = do_sysctl(xch, &sysctl);

    xc_hypercall_bounce_post(xch, schedule);

    return rc;
}

int
xc_sched_arinc653_schedule_get(
    xc_interface *xch,
    uint32_t cpupool_id,
    struct xen_sysctl_arinc653_schedule *schedule)
{
    int rc;
    DECLARE_SYSCTL;
    DECLARE_HYPERCALL_BOUNCE(
        schedule,
        sizeof(*schedule),
        XC_HYPERCALL_BUFFER_BOUNCE_OUT);

    if ( xc_hypercall_bounce_pre(xch, schedule) )
        return -1;

    sysctl.cmd = XEN_SYSCTL_scheduler_op;
    sysctl.u.scheduler_op.cpupool_id = cpupool_id;
    sysctl.u.scheduler_op.sched_id = XEN_SCHEDULER_AAF;
    sysctl.u.scheduler_op.cmd = XEN_SYSCTL_SCHEDOP_getinfo;
    set_xen_guest_handle(sysctl.u.scheduler_op.u.sched_aaf.schedule,
            schedule);

    rc = do_sysctl(xch, &sysctl);

    xc_hypercall_bounce_post(xch, schedule);

    return rc;
}
*/
// ps_rrp do_schedule()
/* Mini Description and comparison between ARINC and PS_RRP
   * ARINC simply picks up the next vcpu to run through the schedule provided
   * However, PS_RRP calcualtes a new schedule and updates domain-timeslice pair
   * according to PS_RRP schedule.
*/
static struct task_slice ps_rrp_do_schedule(const struct scheduler *ops,
					s_time_t now,
					bool_t tasklet_work_scheduled)
{
  struct task_slice ret;
  struct vcpu *new_task = NULL;
  static unsigned int sched_index = 0;
  static s_time_t next_switch_time;
 
  // IF our understanding is right, ops should already have the sorted domain list in it..
  // since adjust_global() has to be invoked before do_schedule()
  struct ps_priv_t *sched_private = SCHED_PRIV(ops);
  const unsigned int cpu = smp_processor_id();
  unsigned long flags;
  // This scheduler is not lock protected right now, make sure to include it in the final stages.... 
  spin_lock_irqsave(&sched_private->lock, flags); 
 if(sched_private->num_schedule_entries < 1)
	sched_private->next_hyperperiod = now + MILLISECS(30);
  else if (now >= sched_private->next_hyperperiod)
  {
	// this means we need to immediately set the new hp to be the length of hp
   sched_private->next_hyperperiod = now + sched_private->hyperperiod;
  // set the counter of number of schedule entries back to 0 if its not
   sched_index = 0;
   next_switch_time = now+sched_private->schedule[0].wcet;
  }
  // focus on scheduling domains within the hyperperiod now
  else {
      while(now >= next_switch_time &&
				sched_index < sched_private->num_schedule_entries)
	{
	sched_index++;
        // update the next_switch time
       next_switch_time = now + sched_private->schedule[sched_index].wcet;
       }
   }
  
  /* Boundary conditions checking phase */
  // If all the domains are exhausted, then set next_switch_time to be @ next major frame
  if(sched_index > sched_private->num_schedule_entries)
  {
	next_switch_time = sched_private->next_hyperperiod;
  }
  
 /* If current task is done with it's execution, point the schedule to next vcpu structure in schedule *ops,
  * OR if we have exhausted all schedule entries within that hyperperiod, run idle timeslices until beginning
  * of next hyperperiod 
  */
  new_task = (sched_index < sched_private->num_schedule_entries)?sched_private->schedule[sched_index].vc
				: IDLETASK(cpu);

  /** ERROR CHECK ON NEW_TASK **/
 // Check to see if this new_task is actually functional -> checking it with
 // vcpu_runnable, if not functional, allocate idle_timeslices to it.
  if(!((new_task!= NULL) && (vcpu_runnable(new_task))
	&& PSVCPU(new_task)))
	new_task = IDLETASK(cpu);

  // Raise a Fatal error if new_task is still empty when program control is @ this point.
  BUG_ON(new_task == NULL);
  // Sanity Check: Make sure none of the hyperperiods are missed
  BUG_ON(now >= sched_private->next_hyperperiod);
  
// unlock the scheduler's lock at this point, not deployed yet ....
  spin_unlock_irqrestore(&sched_private->lock, flags);

   if(tasklet_work_scheduled)
	new_task = IDLETASK(cpu);
 
  // This scheduler does not allow migration to some other PCPU yet since it is only a single core
  // therefore, new_task->processor should always be equal to smp_processor_id() ie cpu

  /* Now all the checks are done and its time to return the amount of time next_schedule_entry(domain) has to run
   * along with the  upcoming task's VCPU structure. */
  ret.time = next_switch_time - now;
  ret.task = new_task;
  // No migration
  ret.migrated = 0;
  printk("Time to be taken for new task to run for with PS_SCHEDULER is:%d\n",ret.time);
  // FATAL error if time is an invalid number
 // BUG_ON(ret.time<=0);
  return ret;
}

// adjust_global
/* Use switch-case with xen_sysctl_scheduler_op to judge which one to go for out of these 2,
 *  XEN_SYSCTL_SCHEDOP_putinfo: invokes aaf_schedule_set()
 *  XEN_SYSCTL_SCHEDOP_getinfo: invokes aaf_schedule_get()
 */
static int aaf_adjust_global(const struct scheduler *ops,
			struct xen_sysctl_scheduler_op *sc)
{
	struct xen_sysctl_aaf_schedule local_sched;
	int rc = -EINVAL;
	switch(sc->cmd)
	{
        // Work in progress for getinfo...
	case XEN_SYSCTL_SCHEDOP_getinfo:
	memset(&local_sched, -1, sizeof(local_sched));
	rc = ps_sched_get(ops, &local_sched);
	if(rc)
	   break;
	if(copy_to_guest(sc->u.sched_aaf.schedule, &local_sched, 1))
		BUG();
	break;
	case XEN_SYSCTL_SCHEDOP_putinfo:
	 if(copy_from_guest(&local_sched, sc->u.sched_aaf.schedule, 1))
	{
		rc = -EINVAL;
		break;
	}
	rc = ps_sched_set(ops, &local_sched);
	break;
	}
	return rc;
}

// Switch_scheduler to change the scheduler on a CPU
static void aaf_switch_sched(struct scheduler *ops,unsigned int cpu,
			     void *pdata, void* vdata)
{
  struct schedule_data *sd = &per_cpu(schedule_data, cpu);
  struct ps_vcpu_t *pvc = vdata;
  ASSERT(!pdata && pvc && is_idle_vcpu(pvc->vc));
  idle_vcpu[cpu]->sched_priv = vdata;
 per_cpu(scheduler, cpu) = ops;
 per_cpu(schedule_data, cpu).sched_priv = NULL; /* no pdata customization for our scheduler */
 // No lock support for now ...
    sd->schedule_lock = &sd->_lock;
}

static int
aaf_pick_cpu(const struct scheduler *ops, struct vcpu *vc)
{
    cpumask_t *online;
    unsigned int cpu;
    printk("aaf cpu_pick invoked for vcpu:%d\n",vc->vcpu_id);
    /* 
     * If present, prefer vc's current processor, else
     * just find the first valid vcpu .
     */
    online = cpupool_domain_cpumask(vc->domain);

    cpu = cpumask_first(online);

    if ( cpumask_test_cpu(vc->processor, online)
         || (cpu >= nr_cpu_ids) )
        cpu = vc->processor;
 
    printk("aaf scheduler running on CPU %d in cpu_pick()\n",cpu);
    return cpu;
}



const struct scheduler sched_aaf_def = {
    .name = "AAF Single Scheduler",
    .opt_name = "aaf",
    .sched_id = XEN_SCHEDULER_AAF,
    .sched_data = NULL,
   /*  .dump_cpu_state = rtpartition_dump_pcpu,  */
 /* .dump_settings  = aaf_dump, */
    .init           = aafsched_init,
    .deinit         = aafsched_deinit,
    .alloc_pdata    = NULL,
    .free_pdata     = NULL,
    .alloc_domdata  = NULL,
    .free_domdata   = NULL,
  /*  .alloc_domdata  = aaf_alloc_domdata,
    .free_domdata   = aaf_free_domdata,
    .destroy_domain = rtpartition_dom_destroy,
    .alloc_vdata    = aaf_alloc_vdata,
    .free_vdata     = aaf_free_vdata,
*/
    .switch_sched   = aaf_switch_sched,
    .insert_vcpu    = NULL,
    .remove_vcpu    = NULL,
    .alloc_vdata    = aafsched_alloc_vdata,
    .free_vdata     = aaf_free_vdata,
    .adjust         = NULL,
    .adjust_global  = aaf_adjust_global,	
    .pick_cpu       = aaf_pick_cpu,
    .do_schedule    = ps_rrp_do_schedule,//aaf_schedule,
    .sleep          = aaf_vcpu_sleep,
    .wake           = aaf_vcpu_wake,
    
    .context_saved  = NULL,
    .yield          = NULL,
    .migrate        = NULL,
};

REGISTER_SCHEDULER(sched_aaf_def);
