#pragma once
#include <setjmp.h>
#include <stdint.h>
/* Flash gia 1 MB: ops dem thao tac (moi flash-word / moi lan xoa); khi ops == fail_at
 * thi "mat dien" (longjmp ve sim_jb), xoa do thi chi xoa 1 nua sector. */
extern uint8_t sim[1024*1024];
extern long ops, fail_at;
extern jmp_buf sim_jb;
extern long overwrite_errors;
