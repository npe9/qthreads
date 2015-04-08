#include <hwloc.h>

#include "qt_subsystems.h"
#include "qt_asserts.h" /* for qassert() */
#include "qt_affinity.h"
#include "qt_debug.h"
#include "qt_envariables.h"
#include "qt_output_macros.h"
#include "shufflesheps.h"


static hwloc_topology_t topology;
static int * tid_to_lid;
static int * lid_to_tid;
static int * lid_to_hid;
static int * lvls;
int arity;
struct {
  hwloc_cpuset_t* binds;
  int num;
} shep;
int * sheps_to_workers;
int num_lids;

static inline void topo_set_tid_to_lid(int first_lvl,
                                       int second_lvl,
                                       int third_lvl) {
    int tid;

    int index[3];

    printf("first_lvl: %d\n", first_lvl);
    printf("second_lvl: %d\n", second_lvl);
    printf("third_lvl: %d\n", third_lvl);

    tid = 0;
    for (index[third_lvl] = 0; index[third_lvl] < lvls[third_lvl]; index[third_lvl]++) {
        for (index[second_lvl] = 0; index[second_lvl] < lvls[second_lvl]; index[second_lvl]++) {
            for (index[first_lvl] = 0; index[first_lvl] < lvls[first_lvl]; index[first_lvl]++) {
                tid_to_lid[tid] = (lvls[0] * lvls[1] * index[2]) + (lvls[0] * index[1]) + (index[0]);
                lid_to_tid[tid_to_lid[tid]] = tid;
                printf("tid_to_lid[%d]: %d\n", tid, tid_to_lid[tid]);

                tid += 1;
            }
        }
    }
}

static inline void topo_set_lid_to_hid(int first_lvl,
                                       int second_lvl,
                                       int third_lvl) {
    int lid;
    int shift;
    int index[3];

    printf("first_lvl: %d\n", first_lvl);
    printf("second_lvl: %d\n", second_lvl);
    printf("third_lvl: %d\n", third_lvl);

#ifdef PLAT_MORGAN_PHI
    shift = 1;
#else
    shift = 0;
#endif

    lid = 0;
    for (index[third_lvl] = 0; index[third_lvl] < lvls[third_lvl]; index[third_lvl]++) {
        for (index[second_lvl] = 0; index[second_lvl] < lvls[second_lvl]; index[second_lvl]++) {
            for (index[first_lvl] = 0; index[first_lvl] < lvls[first_lvl]; index[first_lvl]++) {
                lid_to_hid[lid] = (lvls[0] * lvls[1] * index[2]) + (lvls[0] * index[1]) + (index[0]) + shift;
                printf("lid_to_hid[%d]: %d\n", lid, lid_to_hid[lid]);

                lid += 1;
            }
        }
    }
}

int INTERNAL getTid(int hid, int num_pu, int hw_par){
  int lid = -1, i;
  int tid = -1;
  for(i=0; i<num_pu; i++) if (lid_to_hid[i] == hid) {
    lid = i;
    break;
  }
  printf("lid %d\n", lid);
  for(i=0; i<hw_par; i++) if (tid_to_lid[i] == lid) {
    tid = i;
    break;
  }
  return tid;
}


void INTERNAL qt_affinity_free(void  *ptr,
                               size_t bytes)
{                                      /*{{{ */
    DEBUG_ONLY(hwloc_topology_check(topology));
    hwloc_free(topology, ptr, bytes);
}                                      /*}}} */

void INTERNAL qt_affinity_set(qthread_worker_t *me,
                              unsigned int      nworkerspershep){ 
  hwloc_bitmap_t set;
  int tid;
  int cid;
  int err;
  qthread_worker_id_t num_threads = qthread_readstate(TOTAL_WORKERS);

  tid = me->worker_id;
  
  int new_tid;
  if (1 == arity) {
    new_tid = tid;
  } else {
    int num_part_threads = num_threads / arity; // 7 / 2 = 3
    int num_rem_threads = num_threads % arity;  // 7 % 2 = 1
    int part_cut = num_rem_threads * (num_part_threads + 1); // 1 * (3 + 1) = 4

    int part_size = 0;
    int part_num = 0;
    do {
        part_size += num_part_threads + (part_num < num_rem_threads ? 1 : 0);
        part_num += 1;
    } while (tid >= part_size);
    part_num -= 1;
    printf("T[%d]: part_num: %d\n", tid, part_num);

    if (part_num < num_rem_threads) {
        new_tid = tid - ((num_part_threads + 1) * part_num);
    } else {
        new_tid = tid - (((num_part_threads + 1) * num_rem_threads) + (num_part_threads * (part_num - num_rem_threads)));
    }
    printf("T[%d]: new_tid: %d\n", tid, new_tid);
#ifdef TOPO_BAL_COMP
    new_tid += (num_lids / arity) * part_num;
#elif TOPO_BAL_SCAT

# ifdef PLAT_MORGAN_PHI
    new_tid = (arity * new_tid) + part_num;
    //if (0 < new_tid) {
    //    new_tid = (arity * new_tid) + part_num;
    //    //new_tid += (num_lids / arity / 4) * (part_num + 1);
    //    //new_tid += arity - 1;
    //} else {
    //    //new_tid += (num_lids / arity / 4) * part_num;
    //    new_tid = (arity * new_tid) + part_num;
    //}
# else
    if (new_tid >= (num_lids / arity / 2)) {
        new_tid += (num_lids / arity / 2) * (part_num + 1);
    } else {
        new_tid += (num_lids / arity / 2) * part_num;
    }
# endif
    printf("T[%d]: new_tid: %d\n", tid, new_tid);

#endif
  }

  cid = lid_to_hid[tid_to_lid[new_tid]];

  set = hwloc_bitmap_alloc();
  hwloc_bitmap_only(set, cid);
  err = hwloc_set_cpubind(topology, set, HWLOC_CPUBIND_THREAD);
  err = hwloc_get_last_cpu_location(topology, set, HWLOC_CPUBIND_THREAD);
  cid = hwloc_bitmap_first(set);
  printf("Thread number %d now on %d\n", tid, cid);
  hwloc_bitmap_free(set);
}

void INTERNAL qt_affinity_init(qthread_shepherd_id_t *nbshepherds,
                               qthread_worker_id_t   *nbworkers,
                               size_t                *hw_par)
{
    hwloc_bitmap_t set, set2;
    hwloc_const_bitmap_t cset_available, cset_all;
    hwloc_obj_t obj;
    char *buffer;
    char type[64];
    unsigned i;
    int err, num_pus, num_lids;

#ifdef TOPO_COMP
    fprintf(stderr, "ariel_topo: compact\n");
#elif TOPO_SCAT
    fprintf(stderr, "ariel_topo: scatter\n");
#elif TOPO_BAL_COMP
    fprintf(stderr, "ariel_topo: bal_comp\n");
#elif TOPO_BAL_SCAT
    fprintf(stderr, "ariel_topo: bal_scat\n");
#else
# error "Undefined thread pinning."
#endif

    /* create a topology */
    err = hwloc_topology_init(&topology);
    if (err < 0) {
        fprintf(stderr, "topo_init_failed"); exit(0);
    }
    err = hwloc_topology_load(topology);
    if (err < 0) {
        hwloc_topology_destroy(topology);
        fprintf(stderr, "topo_init_failed"); exit(0);
    }

    /* retrieve the entire set of available PUs */
    cset_available = hwloc_topology_get_topology_cpuset(topology);


    /* retrieve the CPU binding of the current entire process */
    set = hwloc_bitmap_alloc();
    if (!set) {
        fprintf(stderr, "failed to allocate a bitmap\n");
        hwloc_topology_destroy(topology);
        fprintf(stderr, "topo_init_failed"); exit(0);
    }
    err = hwloc_get_cpubind(topology, set, HWLOC_CPUBIND_PROCESS);
    if (err < 0) {
        fprintf(stderr, "failed to get cpu binding\n");
        hwloc_bitmap_free(set);
        hwloc_topology_destroy(topology);
    }

    /* display the processing units that cannot be used by this process */
    if (hwloc_bitmap_isequal(set, cset_available)) {
        printf("this process can use all available processing units in the system\n");
    } else {
        /* compute the set where we currently cannot run.
         * we can't modify cset_available because it's a system read-only one,
         * so we do   set = available &~ set
         */
        hwloc_bitmap_andnot(set, cset_available, set);
        hwloc_bitmap_asprintf(&buffer, set);
        printf("process cannot use %d process units (%s) among %u in the system\n",
                hwloc_bitmap_weight(set), buffer, hwloc_bitmap_weight(cset_available));
        free(buffer);
        /* restore set where it was before the &~ operation above */
        hwloc_bitmap_andnot(set, cset_available, set);
    }
    /* print the smallest object covering the current process binding */
    obj = hwloc_get_obj_covering_cpuset(topology, set);
    hwloc_obj_type_snprintf(type, sizeof(type), obj, 0);
    printf("process is bound within object %s logical index %u\n", type, obj->logical_index);

    /* retrieve the single PU where the current thread actually runs within this process binding */
    set2 = hwloc_bitmap_alloc();
    if (!set2) {
        fprintf(stderr, "failed to allocate a bitmap\n");
        hwloc_bitmap_free(set);
        hwloc_topology_destroy(topology);
        fprintf(stderr, "topo_init_failed"); exit(0);
    }
    err = hwloc_get_last_cpu_location(topology, set2, HWLOC_CPUBIND_THREAD);
    if (err < 0) {
        fprintf(stderr, "failed to get last cpu location\n");
        hwloc_bitmap_free(set);
        hwloc_bitmap_free(set2);
        hwloc_topology_destroy(topology);
    }
    /* sanity checks that are not actually needed but help the reader */
    /* this thread runs within the process binding */
    assert(hwloc_bitmap_isincluded(set2, set));
    /* this thread runs on a single PU at a time */
    assert(hwloc_bitmap_weight(set2) == 1);

    /* print the logical number of the PU where that thread runs */
    /* extract the PU OS index from the bitmap */
    i = hwloc_bitmap_first(set2);
    obj = hwloc_get_pu_obj_by_os_index(topology, i);
    printf("thread is now running on PU logical index %u (OS/physical index %u)\n",
            obj->logical_index, i);

    /* migrate this single thread to where other PUs within the current binding */
    hwloc_bitmap_andnot(set2, set, set2);
    err = hwloc_set_cpubind(topology, set2, HWLOC_CPUBIND_THREAD);
    if (err < 0) {
        fprintf(stderr, "failed to set thread binding\n");
        hwloc_bitmap_free(set);
        hwloc_bitmap_free(set2);
        hwloc_topology_destroy(topology);
    }

    /* reprint the PU where that thread runs */
    err = hwloc_get_last_cpu_location(topology, set2, HWLOC_CPUBIND_THREAD);
    if (err < 0) {
        fprintf(stderr, "failed to get last cpu location\n");
        hwloc_bitmap_free(set);
        hwloc_bitmap_free(set2);
        hwloc_topology_destroy(topology);
    }
    /* print the logical number of the PU where that thread runs */
    /* extract the PU OS index from the bitmap */
#ifdef PLAT_MORGAN_PHI
    i = 1;
#else
    i = 0; //hwloc_bitmap_first(set2);
#endif
    obj = hwloc_get_pu_obj_by_os_index(topology, i);
    printf("thread is running on PU logical index %u (OS/physical index %u)\n",
            obj->logical_index, i);

    hwloc_bitmap_free(set);
    hwloc_bitmap_free(set2);

    /* retrieve the entire set of all PUs */
    cset_all = hwloc_topology_get_complete_cpuset(topology);
    if (hwloc_bitmap_isequal(cset_all, cset_available)) {
        printf("all hardware PUs are available\n");
    } else {
        printf("only %d hardware PUs are available in the machine among %d\n",
                hwloc_bitmap_weight(cset_available), hwloc_bitmap_weight(cset_all));
    }

    {
        num_pus = hwloc_bitmap_weight(cset_available);
        printf("num_pus: %d\n", num_pus);

        tid_to_lid = (int *)malloc(sizeof(int) * num_pus);
        assert(NULL != tid_to_lid);
        lid_to_tid = (int *)malloc(sizeof(int) * num_pus);
        assert(NULL != lid_to_tid);
        lid_to_hid = (int *)malloc(sizeof(int) * num_pus);
        assert(NULL != lid_to_hid);

        /* Set tid to lid mapping */
        int num_lvls = 3;
        num_lids = 1;
        lvls = (int *)malloc(sizeof(int) * num_lvls);
        assert(NULL != lvls);

        int top_lvl;
        int mid_lvl;
        int bot_lvl;

#ifdef PLAT_CURIE
        /* Curie */
        fprintf(stderr, "ariel_plat: curie\n");
        top_lvl = 2; // NUMAs
        mid_lvl = 1; // Modules
        bot_lvl = 0; // Cores

        lvls[top_lvl] = 2;
        lvls[mid_lvl] = 4;
        lvls[bot_lvl] = 2;
#elif PLAT_MORGAN_HOST
        /* Morgan (host) */
        fprintf(stderr, "ariel_plat: morgan_host\n");
        top_lvl = 2; // Sockets
        mid_lvl = 1; // Cores
        bot_lvl = 0; // Hyper-threads

        lvls[top_lvl] = 2;
        lvls[mid_lvl] = 10;
        lvls[bot_lvl] = 2;
#elif PLAT_MORGAN_PHI
        /* Morgan (phi) */
        fprintf(stderr, "ariel_plat: morgan_phi\n");
        top_lvl = 2; // Machine
        mid_lvl = 1; // Cores
        bot_lvl = 0; // Hyper-threads

        lvls[top_lvl] = 56; // Drop one core
        lvls[mid_lvl] = 4;
        lvls[bot_lvl] = 1;
#else
# error "Undefined platform."
#endif

        int i;
        for (i = 0; i < num_lvls; i++) {
            num_lids *= lvls[i];
        }
        printf("num_lids: %d\n", num_lids);
        //assert(num_lids == num_pus);

        /* Set lid to hid mapping */
#ifdef PLAT_CURIE
        /* Curie: assume contiguous PUs share L2, then L3 */
        topo_set_lid_to_hid(bot_lvl, mid_lvl, top_lvl);
#elif PLAT_MORGAN_HOST
        /* Morgan (host) */
        topo_set_lid_to_hid(top_lvl, bot_lvl, mid_lvl);
#elif PLAT_MORGAN_PHI
        /* Morgan (phi) */
        topo_set_lid_to_hid(bot_lvl, mid_lvl, top_lvl);
#else
# error "Undefined platform."
#endif

        /* Set tid to lid mapping */
#if defined(TOPO_COMP) || defined(TOPO_BAL_COMP)
        /* Create compact mapping of lin. thread id space onto 3D 
           logical id space: cores then modules then numas */
# ifdef PLAT_CURIE
        topo_set_tid_to_lid(bot_lvl, mid_lvl, top_lvl);
# elif PLAT_MORGAN_HOST
        topo_set_tid_to_lid(bot_lvl, mid_lvl, top_lvl);
# elif PLAT_MORGAN_PHI
        topo_set_tid_to_lid(bot_lvl, mid_lvl, top_lvl);
# else
#  error "Undefined platform."
# endif
# ifdef TOPO_COMP
        printf("topo_tid_to_lid: compact\n");
        arity = 1;
# else
        printf("topo_tid_to_lid: bal_comp\n");
        arity = lvls[top_lvl];
# endif
#elif defined(TOPO_SCAT) || defined(TOPO_BAL_SCAT)
        /* Create scatter mapping of lin. thraed id space onto 3D
           logical id space: modues then numas then cores */
# ifdef PLAT_CURIE
        topo_set_tid_to_lid(mid_lvl, top_lvl, bot_lvl);
        

# elif PLAT_MORGAN_HOST
        topo_set_tid_to_lid(mid_lvl, top_lvl, bot_lvl);
# elif PLAT_MORGAN_PHI
        topo_set_tid_to_lid(top_lvl, mid_lvl, bot_lvl);
# else
#  error "Undefined platform."
# endif
# ifdef TOPO_SCAT
        printf("topo_tid_to_lid: scatter\n");
        arity = 1;
# else
        printf("topo_tid_to_lid: bal_scat\n");
        arity = lvls[top_lvl];
# endif
#else
# error "No thread to logical id map specified."
#endif

    }

    const char *bindstr = qt_internal_get_env_str("CPUBIND", "NOT_SET");
    if(!bindstr || strcmp(bindstr, "NOT_SET") == 0){
      printf("failed to read cpubind, did you set QT_CPUBIND?\n");
      exit(-1);
    }
    char *bstr = malloc(strlen(bindstr));
    const char *hwparstr = qt_internal_get_env_str("HWPAR", "NOT_SET");
    if(!hwparstr || strcmp(hwparstr, "NOT_SET") == 0){
      printf("failed to read hwpar, did you set QT_HWPAR?\n");
      exit(-1);
    }
    *hw_par = atoi(hwparstr);

    strcpy(bstr,bindstr);
    int j;
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
    
    for(i=0; i<shep.num; i++){
      char tmp[256];
      shep.binds[i] = hwloc_bitmap_alloc();
      hwloc_bitmap_list_sscanf(shep.binds[i], ranges[i]);
      hwloc_bitmap_list_snprintf(tmp, 256, shep.binds[i]);
      printf("Setting shep %d to cpus %s\n", i, tmp);
    }

    int id;
    sheps_to_workers = malloc(sizeof(int) * *hw_par);
    for(i=0; i<shep.num; i++){
      int j = 0, id;
      hwloc_bitmap_foreach_begin(id, shep.binds[i])
        int tid = getTid(id, num_lids, *hw_par);
        printf("getTid(%d, %d, %d) = %d\n", id, num_lids, *hw_par, tid);
        if(tid >= 0){
          sheps_to_workers[i * *hw_par + j++] = tid;
          printf("Setting worker %d to shep %d\n", tid, i);
        }
      hwloc_bitmap_foreach_end();
    }

    *nbshepherds = shep.num;
    *nbworkers = *hw_par / *nbshepherds;
    return;

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

void topo_fini(void) {
  free(lid_to_hid);
  free(tid_to_lid);
  free(lid_to_tid);
  free(lvls);
  hwloc_topology_destroy(topology);
}
