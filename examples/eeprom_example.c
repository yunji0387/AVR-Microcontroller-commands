#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_OVERFLOW 4294967295 // a maximun decimal allow to be stored in a 32-bit unsigned int
#define MAX_POLL 30 // maximum number of pooling allow to do in switch polling


void initial_setup();
void led_on();
void EEPROM_write(unsigned int uiAddress , unsigned char ucData);
void record_time();
void reset_timer(int mode);
void switch_polling(uint8_t led_pos , uint8_t switch_num);
void q2_task();

//overflow_count and elapsed_time set to 32-bit since it is the maximum value allows to write to EEPROM
uint32_t overflow_count; 
uint32_t elapsed_time;
uint8_t led_pattern;    // generate LED pattern to be light up

uint8_t switch_pressed; // check is switch pressed , set to 2 if sw2 is pressed , 3 if sw3 is pressed


int main(void) {

    initial_setup();
    while (1) {
        // infinite loop
        q2_task();
    }

    return 0;
}

ISR(TIMER1_OVF_vect) {
    if(overflow_count < MAX_OVERFLOW){
        overflow_count++;
    }
    reset_timer(0);
}

ISR(INT2_vect) {
    switch_polling(0x04 , 2);
}

ISR(INT3_vect) {
    switch_polling(0x08 , 3);
}

void initial_setup(){
    overflow_count = 0;
    elapsed_time = 0;
    led_pattern = 0xff;

    switch_pressed = 0;

    // Set PORTB as output
    DDRB = 0xff;
    
    // Initially turn off all LEDs
    PORTB = 0xff;

    //set timer to not use prescalling (/1)
    TCCR1B = (1 << CS10);

    //timer counter set to 64535 initially so we get overflow after 1000 count which is 1 ms
    TCNT1H = 0xfc;
    TCNT1L = 0x17;

    //Enable timer overflow interrupt
    TIMSK1 = (1 << TOIE1);

    //set external interrupt for checking any edge
    EICRA = (1 << ISC20) | (1 << ISC30);

    //enable external interrupt INT2 and INT3
    EIMSK = (1 << INT3) | (1 << INT2);

    // Enable global interrupts
    sei();
}

void led_on(){
    PORTB = led_pattern;
}

void EEPROM_write(unsigned int uiAddress , unsigned char ucData){
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE));

    /* Set up address and Data Registers */
    EEAR = uiAddress;
    EEDR = ucData;

    /* Write logical one to EEMPE */
    EECR |= (1<<EEMPE);

    /* Start eeprom write by setting EEPE */
    EECR |=(1<<EEPE);
}

void record_time(){
    elapsed_time = overflow_count;  //record the elpased time ready to be stored to EEPROM

    unsigned char byte1 = (elapsed_time & 0xff000000) >> 24;    // get 24-31 bit value 
    EEPROM_write(0x00 , byte1);

    unsigned char byte2 = (elapsed_time & 0x00ff0000) >> 16;    // get 16-23 bit value
    EEPROM_write(0x02 , byte2);

    unsigned char byte3 = (elapsed_time & 0x0000ff00) >> 8;     // get 8-15 bit value
    EEPROM_write(0x04 , byte3);

    unsigned char byte4 = (elapsed_time & 0x000000ff);          // get 0-7 bit value
    EEPROM_write(0x06 , byte4);
    
}

void reset_timer(int mode){
    if(mode == 1){  // in mode 1 reset the timer
        overflow_count = 0;
    }

    //reset the timer counter to 64535
    TCNT1H = 0xfc;
    TCNT1L = 0x17;
}

void switch_polling(uint8_t led_pos , uint8_t switch_num){
    uint8_t poll_count = 0;
    uint8_t high_level_count = 0;
    uint8_t low_level_count = 0;
    while(poll_count <= MAX_POLL){
        poll_count++;
        if(!(PIND & led_pos)){
            high_level_count++;
            switch_pressed = switch_num;
        }else{
            low_level_count++;
        }
    }

    if(high_level_count > low_level_count){
        led_pattern ^= led_pos;

    }else{
        led_pattern |= led_pos;
    }
}

void q2_task(){
    if(switch_pressed == 2){     //switch2 press
        switch_pressed = 0;
        record_time();
    }else if(switch_pressed == 3){   //switch3 press
        switch_pressed = 0;
        reset_timer(1);
    }
    led_on();
}