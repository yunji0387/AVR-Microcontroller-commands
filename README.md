# AVR-Microcontroller-commands
## commands below tested with STK600 board with ATmega2560 chip without issues.
### [AVR Microcontroller Datasheet](./avr_doc.pdf)


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

### Interrupt programming
- A timer interrupt that fires every 1 millisecond 
```c
#include <avr/io.h>
#include <avr/interrupt.h>

uint8_t interrupt_counter = 0; //  counter uses to count how many interrupt occurred 

//initial setup
void init_setup()
{
	// Set timer 0 to use CTC mode
    TCCR0A = (1 << WGM01);

    // Set timer 0 to use a 64 prescaler
    TCCR0B = (1 << CS01);

    // Set timer 1 compare match 250 ticks
    OCR0A = 125;

    // Enable timer compare match interrupt
    TIMSK0 = (1 << OCIE0A);

    // Enable global interrupts
    sei();

	ADMUX = (1 << REFS1) | (1 << REFS0); //setup for using 2.56 internal voltage reference
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // setup 128 prescaler 
}

int main(void) 
{

  	init_setup();
  	while(1) {
		if(interrupt_counter >= 100){ // update led level every 100ms
        	interrupt_counter = 0;  //reset interrupt counter to 0
			//perform task here		
    	}			
        //perform task here
	}
    return 0;
  }

ISR(TIMER0_COMPA_vect) { // interrupt for heartbeat
    // increment time_counter every 1 millisecond
    interrupt_counter++;
}
```
