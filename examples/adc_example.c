#include <avr/io.h>
#include <avr/interrupt.h>

#define LED_NUM 8
#define VREF 2.56 // internal voltage reference that is using in this code

uint8_t interrupt_counter = 0; //  counter uses to count how many interrupt occurred 
uint8_t past_led_result = 0; // record the past led level for comparison
uint8_t curr_led_result = 0; // current led level

// start the adc convertion and return the adc value 
uint16_t read_adc() 
{
	ADCSRA |= (1 << ADSC); // start convertion
	while(!(ADCSRA & (1 << ADIF))); //wait until adc finished processing
	
	return (ADCL | (ADCH<<8)); // return the adc value
}

//intial setup
void init_setup()
{
	DDRB = 0xff; //set portb as output
  	PORTB = 0xff; // initially off all leds

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

	past_led_result = 0;
	curr_led_result = 0;
}

//calculate voltage input value using adc formula
float convert_to_voltage(uint16_t adc_value)
{
	return ((float)adc_value / (float)1023.0) * (float)VREF;
}

//calculate current led level
uint8_t led_level()
{
	uint16_t result_adc = read_adc(); //get adc value
	float vin = convert_to_voltage(result_adc); //get voltage input
	float vin_percentage = vin/(float)VREF; // get voltage input in percentage (scale)
	return (uint8_t)(vin_percentage * LED_NUM); //ignore decimal since 10% = one led level satisfied, 9.9% still consider < 10%
}

//update led level based on past record, increment/decrement by 1 based on past record
void update_led_level()
{
	uint8_t curr = led_level();		
	//compare current voltage level with the last voltage level to decide to increase/decrease led level
	if(curr > past_led_result && past_led_result < LED_NUM){ 
		past_led_result++;
	}else if(curr < past_led_result && past_led_result > 0){ 	
		past_led_result--;
	}else{
		// past_led_result is same as curr , no update required
	}
}

int main(void) 
{

  	init_setup();
  	while(1) {
		if(interrupt_counter >= 100){ // update led level every 100ms
        	interrupt_counter = 0;  //reset interrupt counter to 0
			update_led_level();		
    	}
		curr_led_result = past_led_result;
		switch (curr_led_result)
		{
		case 0:
			PORTB = 0xff;	//no led light up
			break;
		case 1:
			PORTB = 0xfe;	//1 led light up
			break;
		case 2:
			PORTB = 0xfc;	//2 led light up
			break;
		case 3:
			PORTB = 0xf8;	//3 led light up
			break;
		case 4:
			PORTB = 0xf0;	//4 led light up
			break;
		case 5:
			PORTB = 0xe0;	//5 led light up
			break;
		case 6:
			PORTB = 0xc0;	//6 led light up
			break;
		case 7:
			PORTB = 0x80;	//7 led light up
			break;
		case 8:
			PORTB = 0x00;	//8 led light up
			break;
		default:
			break;
		}				
	}
    return 0;
  }

ISR(TIMER0_COMPA_vect) { // interrupt for heartbeat
    // increment time_counter every 1 millisecond
    interrupt_counter++;
}