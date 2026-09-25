/*
******************************************************************************
**
**  File        : STM32H725RGVX_FLASH_SLOTA.ld
**
**  Abstract    : Linker script ung dung chinh, BUILD VARIANT SLOT A.
**
**                KHAC DUY NHAT SO VOI ban goc STM32H725RGVX_FLASH.ld:
**                  FLASH ORIGIN = 0x08020400 (= OTA_SLOTA_BASE + 1024,
**                                 chua header ota_image_header_t 1024 byte
**                                 - xem ota_map.h. Header do FW_END trong
**                                 fw_update.c ghi rieng, KHONG nam trong
**                                 .bin build ra tu day)
**                  FLASH LENGTH = 0x5FC00 (= 384K - 1024, ~383KB)
**
**  QUAN TRONG VE KICH THUOC HEADER: 1024 byte (KHONG PHAI 64) - day la
**  YEU CAU BAT BUOC cua kien truc ARM (can le SCB->VTOR), khong phai lua
**  chon tuy y. STM32H725 co 179 vector ngat (716 byte), can le toi thieu
**  1024 byte. Neu FLASH ORIGIN o day khong khop CHINH XAC voi OTA_HEADER_
**  SIZE trong ota_map.h, VTOR se bi phan cung "lam tron" sai vi tri ngay
**  khi App bat ngat -> HardFault/treo, loi CHI lo ra luc chay, khong bao
**  gio bao luc build hay luc OTA (CRC32/ACK deu bao thanh cong binh thuong).
**
**                Vi .isr_vector la section DAU TIEN trong SECTIONS (khong
**                doi), no tu dong nam dung tai FLASH ORIGIN moi = dung vi
**                tri VTOR can tro toi (slot_base + 1024). KHONG can sua gi
**                them trong SECTIONS.
**
**  QUAN TRONG: build nay PHAI di kem define -DOTA_MY_SLOT='A' (xem
**              fw_update.h). Neu lech (vd dung linker script nay nhung
**              define OTA_MY_SLOT='B'), fw_update.c se coi Slot A - noi
**              CHINH ANH NAY dang chay - la "slot khong hoat dong" va ghi
**              de len chinh no dang thuc thi. Kiem tra ky truoc khi build.
**
** Copyright (c) 2026 STMicroelectronics.
****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1);    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
  ITCMRAM (xrw)    : ORIGIN = 0x00000000,   LENGTH = 64K
  DTCMRAM (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH    (rx)    : ORIGIN = 0x08020400,   LENGTH = 0x5FC00   /* Slot A + header */
  RAM_D1  (xrw)    : ORIGIN = 0x24000000,   LENGTH = 320K
  RAM_D2  (xrw)    : ORIGIN = 0x30000000,   LENGTH = 32K
  RAM_D3  (xrw)    : ORIGIN = 0x38000000,   LENGTH = 16K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH
  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM_D1 AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM_D1


  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    __dma_buffer_start__ = .;
    *(.dma_buffer)
    *(.dma_buffer.*)
    . = ALIGN(32);
    __dma_buffer_end__ = .;
  } >RAM_D2

  /* Chan loi luc link thay vi luc chay */
  ASSERT(__dma_buffer_end__ <= (ORIGIN(RAM_D2) + LENGTH(RAM_D2)),
         "ERROR: .dma_buffer tran RAM_D2")
  ASSERT(__dma_buffer_start__ >= 0x30000000,
         "ERROR: .dma_buffer khong nam trong D2 SRAM")


  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
