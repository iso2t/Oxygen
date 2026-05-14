#ifndef OXYGEN_KERNEL_KEYBOARD_H
#define OXYGEN_KERNEL_KEYBOARD_H

/* Install the PS/2 keyboard IRQ1 handler and unmask the line at the PIC. */
void keyboard_init(void);

/* Pop a translated ASCII char from the keyboard ring buffer, or -1 if
 * the buffer is empty. Non-blocking. */
int keyboard_getc(void);

#endif
