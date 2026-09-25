#ifndef WDG_MANAGER_H_
#define WDG_MANAGER_H_

typedef enum {
	WDG_ID_LOOP = 0,
	WDG_ID_I2C,
	WDG_ID_COUNT,
} wdg_id_t;

void Wdg_Init(void);
void Wdg_CheckIn(wdg_id_t id);
void Wdg_Task(void);

#endif /* WDG_MANAGER_H_ */
