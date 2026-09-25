#include <comm.h>
#include <fw_update.h>
#include <ina228_driver.h>
#include <led.h>
#include <safety.h>
#include <wdg_manager.h>
#include "main.h"
#include "app_main.h"
#include "cfg_store.h"

extern I2C_HandleTypeDef hi2c1;

#define DCM_PERIOD_MS		10u
#define CFG_VERIFY_MS		250u
#define SELF_CONFIRM_MS		10000u

static ina228_dev_t g_devs[4] = {
		{.addr7 = INA228_ADDR_CH1, .alert_port = DCM_CUR_ALRT1_GPIO_Port, .alert_pin = DCM_CUR_ALRT1_Pin },
		{.addr7 = INA228_ADDR_CH2, .alert_port = DCM_CUR_ALRT2_GPIO_Port, .alert_pin = DCM_CUR_ALRT2_Pin },
		{.addr7 = INA228_ADDR_CH3, .alert_port = DCM_CUR_ALRT3_GPIO_Port, .alert_pin = DCM_CUR_ALRT3_Pin },
		{.addr7 = INA228_ADDR_CH4, .alert_port = DCM_CUR_ALRT4_GPIO_Port, .alert_pin = DCM_CUR_ALRT4_Pin },
};

static struct {
	uint32_t t_dcm, t_scan, t_cfg_verify, t_boot;
	bool self_confirmed;
} g_app;

static bool every(uint32_t *last, uint32_t now, uint32_t period)
{
	const uint32_t dt = now - *last;
	if (dt < period) return false;
	*last = (dt >= 2u * period) ? now : (*last + period);
	return true;
}

static void apply_saved_limits(void)
{
	cfg_limits_payload_t lims;
	if (!CfgStore_ReadLimits(&lims)) return;

	for (uint8_t i = 0; i < INA228_CH_COUNT; i ++) {
		ina228_dev_t *d = ina228_get_dev(i);
		if (d == NULL) continue;
        d->limits.sovl = lims.sovl[i];
        d->limits.bovl = lims.bovl[i];
        d->limits.buvl = lims.buvl[i];
        d->limits.loaded = true;

        (void)ina228_write_limits(d);
        (void)ina228_read_limits(d);
	}
}

static void reinit_stale_dev(uint32_t now)
{
	for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
		if (g_devs[i].cfg_ok)	continue;
		if (!ina228_is_fresh(&g_devs[i], now))	continue;
		ina228_dev_init(&g_devs[i]);
	}
}

static void self_confirmed_task(uint32_t now)
{
	if (g_app.self_confirmed || (uint32_t)(now - g_app.t_boot) < SELF_CONFIRM_MS) return;
	if (CfgStore_WriteBoot(Ota_SlotRunning(), 0u, 1u)) {
		g_app.self_confirmed = true;
	}
}

/*		API		*/

void App_EarlyInit (void)
{
	Wdg_Init();
}

void App_Init(void)
{
	Led_Init();
	Safety_Init();
	FwUpdate_Init();
	CfgStore_Init();

	ina228_bus_init(&hi2c1, g_devs);
	ina228_init_all();
	apply_saved_limits();

	Comm_Init();

	cfg_boot_payload_t bp;
	const ota_slot_t me = Ota_SlotRunning();
	g_app.self_confirmed = (me == OTA_SLOT_NONE) || !CfgStore_ReadBoot(me, &bp) || (bp.confirmed != 0u);
	g_app.t_boot = HAL_GetTick();

	HAL_Delay(5);
}

void App_Loop(void)
{
	const uint32_t now = HAL_GetTick();

	ina228_bus_tick(now);

	if (every(&g_app.t_dcm, now, DCM_PERIOD_MS)) {
		dcm_poll();
		Comm_TxWatchdog(now);
	}

	if (every(&g_app.t_scan, now, INA228_SCAN_PERIOD_MS)) {
		Comm_OnTick();
	}

	Comm_RxPoll();
	Comm_Poll(now);

	if (ina228_bus_state() == INA228_BUS_IDLE) {
		ina228_alert_process();
		Comm_CfgTask();

		if (every(&g_app.t_cfg_verify, now, CFG_VERIFY_MS)) {
			reinit_stale_dev(now);
		}
	}

	Safety_Task(now);
	FwUpdate_Task(now);
	Comm_StatusTask(now);
	Comm_FwPoll();
	Led_Task(now);
	self_confirmed_task(now);

	Wdg_CheckIn(WDG_ID_LOOP);
	if (ina228_bus_state() != INA228_BUS_BUSY) {
		Wdg_CheckIn(WDG_ID_I2C);
	}
	Wdg_Task();
}
