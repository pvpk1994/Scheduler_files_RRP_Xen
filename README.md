# Scheduler_files_RRP_Xen_Pav
The core file of RRP_Xen
------------ Shell Scripts ------------------
uuid_automation.sh :: This file is for AAF scheduler utility to import domain UUID's created under aaf_pool
uuid_arinc.sh:: This file is for ARINC_AAF scheduler utility to import domain UUID's created under arinc_pool 

------------------ C files ------------------
arinc_prefinal.c :: This file runs AAF-generated schedule utilities, but sets these entries in ARINC653 Scheduler in kernel
arinc_sample.c :: A very trivial ARINC scheduler utility file that is Hard real-time agnostic. It just sets the specified entries
                  in a First-come-first-serve basis.
aaf_prefinal.c :: This file runs the AAF-generated schedule utilities and sets the entries in AAF-run Xen schdeuler file
