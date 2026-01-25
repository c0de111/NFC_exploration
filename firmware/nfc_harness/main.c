#include <avr/io.h>
#include <util/delay.h>

static inline void clock_init(void)
{
    // 20 MHz internal oscillator, prescaler disabled
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0);
}

int main(void)
{
    clock_init();

    // Simple bring-up indicator: toggle PA2 (adjust to your board LED)
    PORTA.DIRSET = PIN2_bm;
    PORTA.OUTCLR = PIN2_bm;

    while (1)
    {
        PORTA.OUTTGL = PIN2_bm;
        _delay_ms(250);
    }
}
