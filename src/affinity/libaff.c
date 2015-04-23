#include <hwloc.h>
#include <stdio.h>

#include "qt_affinity.h"
#include "qt_envariables.h"

hwloc_topology_t topology;

// Shepherd affinity 
struct {
  hwloc_cpuset_t* binds;
  int num;
} shep;

// Worker affinity bindings, indexed by worker_id
struct {
  hwloc_cpuset_t* binds;
  int num;
} workers;

int qt_get_unique_id(int i){
  return i + 1;
}

void INTERNAL qt_affinity_init(qthread_shepherd_id_t *nbshepherds,
                               qthread_worker_id_t   *nbworkers,
                               size_t                *hw_par)
{                           
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  const char *bindstr = qt_internal_get_env_str("CPUBIND", "NOT_SET");
  if(!bindstr || strcmp("NOT_SET", bindstr) == 0){
    hwloc_const_bitmap_t allowed = hwloc_bitmap_dup(hwloc_topology_get_allowed_cpuset(topology));
    shep.num = 1;
    if(!allowed){
      printf("hwloc detection of allowed cpus failed\n");
      exit(-1);
    }
    workers.num = hwloc_bitmap_weight(allowed);
    workers.binds = malloc(sizeof(hwloc_cpuset_t) * workers.num);
    for(int i = 0; i< workers.num; i++){
      workers.binds[i] = hwloc_bitmap_alloc();  
      workers.binds[i] = hwloc_bitmap_dup(allowed);
    }
    
  } else {
    char *bstr = malloc(strlen(bindstr));
    strcpy(bstr,bindstr);
    int i,j;
    shep.num = 1; 
    i = 0;
    while(bstr[i] != 0){
      if(bstr[i] == ':') shep.num ++;
      i++;
    }
    char ** ranges = malloc(sizeof(char**) * shep.num);
    j = 0;
    ranges[j++] = bstr; 
    for(i=0; bstr[i] != 0; i++){
      if(bstr[i] == ':'){
        bstr[i] = 0;
        ranges[j++] = bstr+i+1;
      }
    }

    shep.binds = malloc(sizeof(hwloc_cpuset_t) * shep.num);
    workers.num = 0;
    for(i=0; i<shep.num; i++){
      char tmp[256];
      shep.binds[i] = hwloc_bitmap_alloc();
      hwloc_bitmap_list_sscanf(shep.binds[i], ranges[i]);
      hwloc_bitmap_list_snprintf(tmp, 256, shep.binds[i]);
      printf("Setting shep %d to cpus %s\n", i, tmp);
      workers.num += hwloc_bitmap_weight(shep.binds[i]);
    }
    j = 0;
    int id;
    workers.binds = malloc(sizeof(hwloc_cpuset_t) * workers.num);
    for(i=0; i<shep.num; i++){
      char tmp[256];
      hwloc_bitmap_foreach_begin(id, shep.binds[i])
        workers.binds[j] = hwloc_bitmap_alloc();
        hwloc_bitmap_set(workers.binds[j], id);
        hwloc_bitmap_list_snprintf(tmp, 256, workers.binds[j]);
        printf("Setting worker %d to cpu %s\n", j, tmp);
        j++;
      hwloc_bitmap_foreach_end();
    }
  }
  *nbshepherds = shep.num;
  *nbworkers = workers.num / shep.num;
  *hw_par = workers.num; 
}

void INTERNAL qt_affinity_set(qthread_worker_t *me,
                              unsigned int      nworkerspershep)
{                                                                                             
  hwloc_set_cpubind(topology, workers.binds[me->worker_id], HWLOC_CPUBIND_THREAD);
}                                     

int INTERNAL qt_affinity_gendists(qthread_shepherd_t   *sheps,
                                  qthread_shepherd_id_t nshepherds)
{                                                                     

  for (size_t i = 0; i < nshepherds; ++i) {
      sheps[i].node            = i;
      sheps[i].sorted_sheplist = calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
      sheps[i].shep_dists      = calloc(nshepherds, sizeof(unsigned int));
  }
  for (size_t i = 0; i < nshepherds; ++i) {
    for (size_t j = 0, k = 0; j < nshepherds; ++j) {
      if (j != i) {
        sheps[i].shep_dists[j] = 10;
        //qthread_debug(AFFINITY_DETAILS, "pretending distance from %i to %i is %i\n", (int)i, (int)j, (int)(sheps[i].shep_dists[j]));
        sheps[i].sorted_sheplist[k++] = j;
      }
    }
  }

  return QTHREAD_SUCCESS;
}                             

/* vim:set expandtab: */
