#include <comm.h>
#include <fw_update.h>
#include <ina228_driver.h>
#include <led.h>
#include <safety.h>
#include <self_test.h>
#include <wdg_manager.h>
#include "main.h"
#include "app_main.h"
#include "cfg_store.h"
#include "psu_cfg.h"
#include "flash_guard.h"

extern I2C_HandleTypeDef hi2c1;

#define DCM_PERIOD_MS		10u
#define CFG_VERIFY_MS		250u
#define CONFIRM_RETRY_MS	1000u   /* ghi confirm loi thi thu lai moi chung nay */

/* INA1..3 = DCM1..3 cua cum 1. INA4 tren mach bo qua hoan toan. */
static ina228_dev_t g_devs[INA228_CH_COUNT] = {
		{.addr7 = INA228_ADDR_CH1, .alert_port = DCM_CUR_ALRT1_GPIO_Port, .alert_pin = DCM_CUR_ALRT1_Pin },
		{.addr7 = INA228_ADDR_CH2, .alert_port = DCM_CUR_ALRT2_GPIO_Port, .alert_pin = DCM_CUR_ALRT2_Pin },
		{.addr7 = INA228_ADDR_CH3, .alert_port = DCM_CUR_ALRT3_GPIO_Port, .alert_pin = DCM_CUR_ALRT3_Pin },
};

static struct {
	uint32_t t_dcm, t_scan, t_cfg_verify, t_confirm_try;
	bool self_confirmed;
} g_app;

static bool every(uint32_t *last, uint32_t now, uint32_t period)
{
	const uint32_t dt = now - *last;
	if (dt < period) return false;
	*last = (dt >= 2u * period) ? now : (*last + period);
	return true;
}

/* Nap 9 thanh ghi hieu luc (PsuCfg_InaRegs) vao driver cho ca 4 kenh; reinit = true thi khoi tao lai IC. */
static void load_ina_regs(bool reinit)
{
	for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
		uint16_t r[INA228_NREGS];
		PsuCfg_InaRegs(i, r);
		ina228_set_regs(&g_devs[i], r);
		if (reinit) (void)ina228_dev_init(&g_devs[i]);
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
	if (g_app.self_confirmed) return;

	/* Dieu kien "chay duoc" (xem self_test.h): cam bien song + safety da khoi dong,
	 * lien tuc SELFTEST_WINDOW_MS. Khong chi doi thoi gian troi qua. */
	selftest_in_t in = { 0u, Safety_State(0u), Comm_PcLinkAlive(now) };
	for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
		if (ina228_is_fresh(&g_devs[i], now) && g_devs[i].cfg_ok) in.ch_ok++;
	}
	if (!SelfTest_Update(now, &in)) return;

	/* Xoa sector chan CPU (ke ca EXTI trip). Khi dau ra dang bat chi ghi neu
	 * khong phai xoa; neu can xoa thi doi den khi nguon tat. */
	if (Safety_AnyPowerOn() && CfgStore_NeedsErase()) return;

	/* Ghi loi thi thu lai toi da 1 lan/giay (moi lan loi ton ban ghi flash) */
	if ((uint32_t)(now - g_app.t_confirm_try) < CONFIRM_RETRY_MS) return;
	g_app.t_confirm_try = now;

	if (CfgStore_WriteBoot(Ota_SlotRunning(), 0u, 1u)) {
		g_app.self_confirmed = true;
	}
}

/*		API		*/

void App_EarlyInit (void)
{
	FlashGuard_Init();
	Wdg_Init();
}

void App_Init(void)
{
	Led_Init();
	Safety_Init();
	FwUpdate_Init();
	CfgStore_Init();

	PsuCfg_Load();                       /* cau hinh PSU tu Flash (hoac mac dinh) + nguong Supply */
	load_ina_regs(false);

	ina228_bus_init(&hi2c1, g_devs);
	ina228_init_all();

	Comm_Init();

	cfg_boot_payload_t bp;
	const ota_slot_t me = Ota_SlotRunning();
	g_app.self_confirmed = (me == OTA_SLOT_NONE) || !CfgStore_ReadBoot(me, &bp) || (bp.confirmed != 0u);
	SelfTest_Reset();

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

	/* Quet 4 INA bang I2C DMA. Firmware KHONG tu phat telemetry: PC hoi (02 02) thi tra loi tu du lieu moi nhat. */
	if (every(&g_app.t_scan, now, INA228_SCAN_PERIOD_MS) && !FwUpdate_IsBusy()) {
		(void)ina228_bus_start();
	}

	Comm_RxPoll();

	if (ina228_bus_state() == INA228_BUS_DONE) {
		ina228_bus_collect(NULL);
	}

	if (ina228_bus_state() == INA228_BUS_IDLE) {
		ina228_alert_process();

		/* Cau hinh moi (04 04, chi nhan khi moi cum OFF): nap lai thanh ghi cho INA */
		if (PsuCfg_ConsumeDirty()) {
			load_ina_regs(true);
		}

		/* Nguong DUOI cua INA (SUVL, BUVL) chi bat khi cum chua INA do dang SAFE_ON */
		for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
			const uint8_t c = Safety_ClusterOfIna(i);
			ina228_set_under_armed(&g_devs[i], c != 0xFFu && Safety_IsArmed(c));
		}
		(void)ina228_limits_sync();

		if (every(&g_app.t_cfg_verify, now, CFG_VERIFY_MS)) {
			reinit_stale_dev(now);
		}
	}

	Safety_Task(now);
	FwUpdate_Task(now);
	Comm_FwPoll();
	Led_Task(now);
	self_confirmed_task(now);

	Wdg_CheckIn(WDG_ID_LOOP);
	if (ina228_bus_state() != INA228_BUS_BUSY) {
		Wdg_CheckIn(WDG_ID_I2C);
	}
	Wdg_Task();
}
