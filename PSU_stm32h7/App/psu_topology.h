#ifndef PSU_TOPOLOGY_H_
#define PSU_TOPOLOGY_H_

/* Kien truc he thong: 1 PSU = 1 STM32 quan ly 4 cum DCM (+ 1 cum Supply).
 * Moi cum co 3 DCM va MOT chan EN chung => 4 may trang thai safety doc lap.
 * Mach bring-up hien tai chi co chan EN cua cum 1 (cum 2..4 chua co phan cung). */
#define PSU_CLUSTERS            4u
#define PSU_DCM_PER_CLUSTER     3u

#endif /* PSU_TOPOLOGY_H_ */
