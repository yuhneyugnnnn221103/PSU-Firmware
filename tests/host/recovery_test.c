/* Test bootloader recovery: giao thuc FW_* + ghi slot + mat dien tai tung thao tac. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "flash_sim.h"
#include "boot_recovery.h"
#include "ota_proto.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "crc16_ccitt.h"
#include "crc32_sw.h"

/* ---- phan cung gia ---- */
static uint8_t  tx[8][OTAP_ACK_SZ]; static int ntx; static bool reset_called;
void RecHw_Init(void) {}
bool RecHw_RxByte(uint8_t p, uint8_t *b) { (void)p; (void)b; return false; }
void RecHw_Tx(uint8_t p, const uint8_t *buf, uint16_t len) { (void)p; if (ntx < 8 && len == OTAP_ACK_SZ) memcpy(tx[ntx++], buf, len); }
void RecHw_ResetSystem(void) { reset_called = true; }

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static void feed_frame(const uint8_t *f, uint16_t n) { for (uint16_t i=0;i<n;i++) Boot_RecoveryFeed(0, f[i]); }

static uint16_t mkframe(uint8_t *f, uint8_t cmd, const uint8_t *payload, uint16_t plen, uint16_t total)
{
    memset(f, 0, total);
    f[0]=OTAP_HDR1; f[1]=OTAP_HDR2; f[2]=cmd; f[3]=OTAP_BOARD_ADDR;
    memcpy(&f[4], payload, plen);
    uint16_t crc = Crc16_Compute(&f[2], total-4-2);
    f[total-4]=crc>>8; f[total-3]=crc; f[total-2]=OTAP_TAIL1; f[total-1]=OTAP_TAIL2;
    return total;
}
static void be32w(uint8_t *d, uint32_t v){ d[0]=v>>24; d[1]=v>>16; d[2]=v>>8; d[3]=v; }

/* gui lenh, tra ve status cua ACK cuoi (255 neu khong co ACK) */
static int ack_status(void){ return ntx ? tx[ntx-1][OTAP_ACK_OFF_STATUS] : 255; }

static void send_begin(uint32_t size, uint32_t crc, uint32_t ver)
{ uint8_t p[12], f[OTAP_BEGIN_SZ]; be32w(p,size); be32w(p+4,crc); be32w(p+8,ver); ntx=0; mkframe(f,OTAP_CMD_FW_BEGIN,p,12,OTAP_BEGIN_SZ); feed_frame(f,OTAP_BEGIN_SZ); }
static void send_data(uint16_t seq, const uint8_t *d, uint16_t len)
{ uint8_t p[4+OTAP_CHUNK_MAX]={0}, f[OTAP_DATA_FRAME_SZ]; p[0]=seq>>8;p[1]=seq;p[2]=len>>8;p[3]=len; memcpy(p+4,d,len); ntx=0; mkframe(f,OTAP_CMD_FW_DATA,p,sizeof(p),OTAP_DATA_FRAME_SZ); feed_frame(f,OTAP_DATA_FRAME_SZ); }
static void send_ctrl(uint8_t cmd){ uint8_t f[OTAP_CTRL_SZ]; ntx=0; mkframe(f,cmd,NULL,0,OTAP_CTRL_SZ); feed_frame(f,OTAP_CTRL_SZ); }

static uint8_t img[1000];
static uint32_t img_crc;

static void build_image(ota_slot_t s)
{
    for (unsigned i=0;i<sizeof img;i++) img[i]=(uint8_t)(i*7+3);
    uint32_t msp=0x24050000u, rh=(Ota_CodeBase(s)+0x101u);   /* Thumb */
    memcpy(img,&msp,4); memcpy(img+4,&rh,4);
    img_crc = Crc32_Compute(img,sizeof img);
}

/* Chay 1 phien day du. Tra ve true neu toi COMMIT thanh cong. */
static bool full_session(void)
{
    reset_called=false;
    send_begin(sizeof img,img_crc,0x0102u); if (ack_status()!=OTAP_ST_OK) return false;
    for (uint16_t seq=0; (uint32_t)seq*OTAP_CHUNK_MAX < sizeof img; seq++) {
        uint32_t off=(uint32_t)seq*OTAP_CHUNK_MAX, n=sizeof img-off; if(n>OTAP_CHUNK_MAX)n=OTAP_CHUNK_MAX;
        send_data(seq,img+off,(uint16_t)n); if (ack_status()!=OTAP_ST_OK) return false;
    }
    send_ctrl(OTAP_CMD_FW_END);    if (ack_status()!=OTAP_ST_OK) return false;
    send_ctrl(OTAP_CMD_FW_COMMIT); return ack_status()==OTAP_ST_OK && reset_called;
}

static void reboot(void){ CfgStore_Init(); Boot_RecoveryResync(0); }

int main(void)
{
    memset(sim,0xFF,sizeof sim); CfgStore_Init();
    build_image(OTA_SLOT_A);

    /* 1. INFO bao 'R' */
    send_ctrl(OTAP_CMD_FW_INFO);
    CHECK(ntx==1 && tx[0][OTAP_ACK_OFF_SLOT]=='R', "INFO phai tra 'R'");

    /* 2. Tu choi truoc khi BEGIN / sai thu tu / sai slot / sai kich thuoc */
    send_data(0,img,256); CHECK(ack_status()==OTAP_ST_BUSY,"DATA khi chua BEGIN");
    send_ctrl(OTAP_CMD_FW_COMMIT); CHECK(ack_status()==OTAP_ST_NO_COMMIT,"COMMIT khi chua verify");
    send_begin(0,0,0); CHECK(ack_status()==OTAP_ST_BAD_SIZE,"size 0");
    send_begin(OTA_IMG_MAX_SIZE+1,0,0); CHECK(ack_status()==OTAP_ST_BAD_SIZE,"size qua lon");
    send_begin(sizeof img,img_crc,1); CHECK(ack_status()==OTAP_ST_OK,"begin");
    send_data(1,img+256,256); CHECK(ack_status()==OTAP_ST_BAD_SEQ,"seq nhay coc");
    uint8_t bad[OTAP_CHUNK_MAX]; memcpy(bad,img,sizeof bad); uint32_t rh=0x08000101u; memcpy(bad+4,&rh,4);
    send_data(0,bad,256); CHECK(ack_status()==OTAP_ST_WRONG_SLOT,"reset handler ngoai slot");
    send_data(0,img,256); CHECK(ack_status()==OTAP_ST_BUSY,"sau WRONG_SLOT phien phai huy");

    /* 3. CRC sai o END */
    send_begin(sizeof img,img_crc^1u,1); CHECK(ack_status()==OTAP_ST_OK,"begin 2");
    for (uint16_t s=0;s<4;s++){ uint32_t off=s*256u,n=sizeof img-off; if(n>256)n=256; send_data(s,img+off,(uint16_t)n); }
    send_ctrl(OTAP_CMD_FW_END); CHECK(ack_status()==OTAP_ST_CRC_FAIL,"crc sai");
    CHECK(!Ota_ImageValid(OTA_SLOT_A),"khong duoc co anh hop le sau CRC sai");

    /* 4. Phien dung -> anh hop le + boot record + COMMIT reset */
    CHECK(full_session(),"phien day du");
    CHECK(Ota_ImageValid(OTA_SLOT_A) && Ota_VectorOk(OTA_SLOT_A),"slot A hop le");
    cfg_boot_payload_t bp; CHECK(CfgStore_ReadBoot(OTA_SLOT_A,&bp)&&bp.boot_count==0&&!bp.confirmed,"boot record");
    CHECK(overwrite_errors==0,"khong duoc ghi de flash (%ld)",overwrite_errors);

    /* 5. Slot B: anh link cho B */
    build_image(OTA_SLOT_B); memset(sim,0xFF,sizeof sim); CfgStore_Init();
    CHECK(full_session(),"phien slot B");
    CHECK(Ota_ImageValid(OTA_SLOT_B) && !Ota_ImageValid(OTA_SLOT_A),"chi slot B hop le");

    /* 6. Quet mat dien o MOI thao tac flash cua mot phien */
    build_image(OTA_SLOT_A);
    long total=0;
    for (long k=0;;k++) {
        memset(sim,0xFF,sizeof sim); CfgStore_Init(); ops=0; fail_at=k; bool crashed=false; bool ok=false;
        if (setjmp(sim_jb)==0) ok=full_session(); else crashed=true;
        fail_at=-1;
        if (!crashed) { total=k; CHECK(ok,"phien khong crash phai OK"); break; }
        reboot();
        /* Sau mat dien: anh hop le neu va chi neu noi dung dung y het */
        if (Ota_ImageValid(OTA_SLOT_A)) {
            CHECK(memcmp((const void*)(sim+(Ota_CodeBase(OTA_SLOT_A)-0x08000000u)),img,sizeof img)==0,"k=%ld anh hop le nhung sai noi dung",k);
        }
        /* Van thu lai duoc */
        CHECK(full_session(),"k=%ld khong the thu lai sau mat dien",k);
        CHECK(Ota_ImageValid(OTA_SLOT_A)&&Ota_VectorOk(OTA_SLOT_A),"k=%ld sau retry slot A khong hop le",k);
    }
    CHECK(overwrite_errors==0,"ghi de flash sau sweep (%ld)",overwrite_errors);
    printf("recovery sweep: %ld diem mat dien, fails=%d\n", total, fails);
    return fails?1:0;
}
