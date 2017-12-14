#include "RFM69.h"

extern RFM69 radio;
extern IRQWrapper irq;

void board_init();
void extcb1(EXTDriver *extp, expchannel_t channel);