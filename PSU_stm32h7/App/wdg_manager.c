#include <wdg_manager.h>
#include "iwdg_hw.h"

#define WDG_ALL_MASK	((1u << WDG_ID_COUNT) - 1u)

static uint32_t g_wdg_alive;

void Wdg_Init(void)
{
	IWDG_Start(WDG_TIMEOUT_MS);
	g_wdg_alive = 0;
}

void Wdg_CheckIn(wdg_id_t id)
{
	g_wdg_alive |= (1u << id);
}

void Wdg_Task(void)
{
	if ((g_wdg_alive & WDG_ALL_MASK) == WDG_ALL_MASK) {
		IWDG_Refresh();
		g_wdg_alive = 0;
	}
}
