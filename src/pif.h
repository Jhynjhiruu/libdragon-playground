#pragma once

#include <libdragon.h>

void hang_pif(void (*reset_callback)(), void (*setup_callback)(void));

__attribute__((noreturn)) void resume_ipl1(void);
