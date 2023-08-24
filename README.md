# AVR Microcontroller Notes and Examples
- For STK600 board and ATmega2560 chips

### [AVR Microcontroller Datasheet](./avr_doc.pdf)
### [My Notes](./notes/)
### [Code examples](./examples/)

### How to connect to microcontroller(Windows system)
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

- Install WSL(Windows Subsystem for Linux) and Ubuntu LTS
- Connect the STK600 board power source USB to your computer
- Connect Atmel ICE to the STK600 board's JTAG port and USB side to your computer
- Now that both devices(Atmel ICE and STK600) are only connected to your local computer, WSL has not yet attached to both devices yet
- To allow WSL to attach to Atmel ICE and STK600
  1. Run Windows power shell/command prompt as administrator mode
  2. Run command below to check the bus ID of Atmel ICE and STK600
     ```bash
     usbipd wsl list
     ```
  3. Once you get both devices' bus ID, run:
     ```bash
     usbipd wsl attach -b <busid>
     ```
     command above will ask for sudo permission, enter password to allow device connect to WSL.
  4. To check if both devices(Atmel ICE and STK600) connected to WSL, run command below on WSL:
     ```bash
     lsusb
     ```
  5. To detach the devices, run:
     ```c
     usbipd wsl detach --busid <busid>
     ``` 

<!-- /MarkdownTOC -->
</details>

### How to transfer files from your local computer to WSL
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

```bash
cp <path to file in your local computer> <path for files to be store on your WSL>
```
- example
```bash
cp /mnt/c/Users/AA/Desktop/avr_code.c /home/aa
```

<!-- /MarkdownTOC -->
</details>

### How to run
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

1. Make sure to run your WSL as administrator
2. Make sure to setup a Makefile that will transfer your code to the microcontroller, sample Makefile file included in this repository
3. Transfer your code and Makefile to the WSL
4. If you are using the sample Makefile, run the below commands:
   - First run make to build the executable
     ```bash
     make
     ```
   - Then transfer code to microcontroller
     ```bash
     make <name you set on Makefile>-install
     ```
     - If you get the error below:
       [make error](./images/make_error.png)
       Please use the command below
       ```bash
       sudo make <name you set on Makefile>-install
       ```

<!-- /MarkdownTOC -->
</details>

### Basic code examples
## codes below are run with STK600 board with ATmega2560 chip.
#### Basic setup code to run the microcontroller
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

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

<!-- /MarkdownTOC -->
</details>

#### Turn on LEDs on STK600 board
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

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

<!-- /MarkdownTOC -->
</details>

#### Basic Timer
<details close>
<summary><b>(click to expand/hide)</b></summary>
<!-- MarkdownTOC -->

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
- To calculate to fire interrupt at an exact time, visit link below:
  - [AVR Timer Calculator](https://eleccelerator.com/avr-timer-calculator/)

<!-- /MarkdownTOC -->
</details>

#### Hysteresis

#### Analog I/O

#### Pulse Width Modulation(PWM)

#### Communication

#### Control System & PID

#### Scheduling Algorithm
