# AVR-Microcontroller-commands
## commands below tested with STK600 board with ATmega2560 chip without issues.

### Basic setup to run the microcontroller
```c
#include <avr/io.h>

int main(void)
{
  while (1) {
        // infinite loop
        // perform task(s) here
    }
  return 0;
}
```

### Turn on LEDs on STK600 board
- To turn LEDs, first set portB as output then change the 8bit value on variable PORTB
  - each bit on the PORTB variable represent a LED
    - 0 : turn on LED
    - 1 : turn off LED
```c
#include <avr/io.h>

int main(void) {
    //Set PORTB as output
    DDRB = 0xff;

    PORTB = 0x00; //turn on all LEDs
    
    while (1) {
        // infinite loop
        // perform task(s) here
    }

    return 0;
}
```
