#ifndef OXYGEN_KERNEL_PIC_H
#define OXYGEN_KERNEL_PIC_H

#include <stdint.h>

/* Initialize and remap the legacy 8259 PIC pair. After this, IRQ0..7 fire
 * on vectors `master_offset..master_offset+7` and IRQ8..15 fire on
 * `slave_offset..+7`. All lines start masked. */
void pic_remap(uint8_t master_offset, uint8_t slave_offset);

void pic_send_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif
