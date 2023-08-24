#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// timer 0 resolution -- conveniently set to overflow every 0.002048s with a 1MHz clock
// used for process timing
#define TIMER0_SELECT      2
#define PROCESS_OVERFLOWS  50   // 20x/s
// at 20x/s and 100 samples, we have a 5s sliding window
#define NUM_SAMPLE  100

#define LED_NUM 8
#define VREF 2.56 // internal voltage reference that is using in this code

#define ON_LIMIT  2
#define ON_MAX    2
#define OFF_LIMIT 1
#define OFF_MIN   1

#define KEY_REPEAT   2

#define CYCLE_STEP   5

//Tuned Gain value , all value are multiply by 100000 to get rid of decimal, will divide it back after PID calculation
#define P_GAIN 100000 //1.0
#define I_GAIN 22     //0.00022
#define D_GAIN 98000  // 0.98
#define GAIN_MULTIPLYER 100000

//Bounds for limiting large error jump
#define I_STATE_MIN_BOUND -50
#define I_STATE_MAX_BOUND 50
#define MAX_NEGATIVE_ERROR_JUMP -20
#define MAX_POSITIVE_ERROR_JUMP 20

enum BOOL 
{
  false,
  true
};
typedef enum BOOL boolean;

enum STATES
{
  NONE,
  KEY0_ON,
  KEY1_ON,
  KEY2_ON,
  NUM_STATES
};

typedef enum STATES State;

struct KEY_INFO
{
  unsigned int count;
  boolean      state;
};

typedef struct KEY_INFO KeyInfo;

// function pointer for defining our state machine lookup table
typedef State(*getNextState)(KeyInfo *keyInfo);

// variables used by interrupts
volatile unsigned int overflows = 0;
volatile unsigned char curr_duty = 0x00;

int cycle_step = CYCLE_STEP; //duty cycle

uint8_t past_led_result = 0; // record the past led level for comparison

//array for storing light sensor input into sliding windows
unsigned short light[NUM_SAMPLE];
unsigned short light_index = 0;
unsigned short light_samples = 0;
unsigned long avg_light = 0;

uint8_t set_point = 0; 
signed long iState = 0;
uint8_t dState = 0;

void reset_timer2()
{
  TIMSK2 &= ~_BV(TOIE2);

  // set to fast PWM count to OCR2B match and clear output on compare match (for PH6) with an 8-bit timer running at CPU clock speed
  TCCR2A = 0x23;
  TCCR2B = 0x01;

  TCNT2 = 0;
  OCR2B = curr_duty;

  TIFR2 |= _BV(TOV2);
  TIMSK2 |= _BV(TOIE2);
}

void reset_timer0()
{
  // start at 131 to give us .001s per overflow (or 1ms)
  TCNT0 = 131;
  overflows = 0;
  TCCR0B = TIMER0_SELECT;
  TIMSK0 |= _BV(TOIE0);
}

// start the adc convertion and return the adc value 
uint16_t read_adc() 
{
	ADCSRA |= (1 << ADSC); // start convertion
	while(!(ADCSRA & (1 << ADIF))); //wait until adc finished processing

	ADCSRA |= _BV(ADIF);
	
	return (ADCL | (ADCH<<8)); // return the adc value
}

//calculate voltage input value using adc formula
float convert_to_voltage(uint16_t adc_value)
{
	return ((float)adc_value / (float)1023.0) * (float)VREF;
}

//update adc value into sliding window array
void update_ADC()
{
	uint16_t result_adc = read_adc(); //read 10-bit adc value
	float currVoltageLevel = convert_to_voltage(result_adc);	//convert from 10-bit adc value to voltage between 0v - 2.56v
	uint8_t currKPH = (((currVoltageLevel/(float)VREF)*1023)/10); //convert from voltage to kph
	// replace oldest value with newest for the running averaging
	avg_light -= light[light_index];
	light[light_index] = currKPH;
	avg_light += light[light_index];
	
	// stop counting after we have full history
	if ( light_samples < NUM_SAMPLE )
	{
		light_samples++;
	}
	
	light_index = (light_index+1)%NUM_SAMPLE;
}

//updatePID
//return : final PID offset value
int16_t updatePID(int16_t error, uint8_t state)
{
  int32_t pTerm = (P_GAIN * error)/GAIN_MULTIPLYER;
  
  //eliminate large error jump
  if(error > MAX_NEGATIVE_ERROR_JUMP && error < MAX_POSITIVE_ERROR_JUMP){
    iState += error;
    //make sure iState stay in the bound to avoid iState become a huge value
    if(iState < I_STATE_MIN_BOUND || iState > I_STATE_MAX_BOUND){
      iState -= error;
    }
  }
  int32_t iTerm = (I_GAIN * iState)/GAIN_MULTIPLYER;

  int32_t dTerm = (D_GAIN * (dState - state))/GAIN_MULTIPLYER;
  dState = state; //update past state
  return (pTerm + iTerm + dTerm);
}

//pid_controller
//description: update duty cycle with current state and PID result, then output curr feedback into LED
void pid_controller()
{
  uint8_t state = (avg_light/light_samples); //curr state
  int16_t error = set_point - state;
  int16_t output = updatePID(error, state);
  
  if(set_point > 0){ // only process when set point is > 0
    curr_duty = state + output;
  }

  PORTB = ~(curr_duty);

}

// let's do some hysteresis!
void sample_inputs(KeyInfo *keyInfo)
{
  unsigned char keys = PIND;  // store now so we don't get changes as we go...
  unsigned char key;
  
  for ( key=PD0 ; key <= PD2 ; key++ )
  {
    if ( (keys & _BV(key)) == 0 )
    {
      if ( keyInfo[key].count < ON_MAX )
        keyInfo[key].count++;
    }
    else if ( keyInfo[key].count > OFF_MIN )
      keyInfo[key].count--;
    
    if ( keyInfo[key].count >= ON_LIMIT )
      keyInfo[key].state = true;
    else if ( keyInfo[key].count <= OFF_LIMIT )
      keyInfo[key].state = false;
  }
}

///////////////////////////////////////////////
// Handlers for processing state machine states
// note: not required in submitted solutions, just showing how it's done

State currStateNone(KeyInfo *keyInfo)
{
  State next_state = NONE;

  if ( keyInfo[0].state ) // set speed to 5 kph
  {
    next_state = KEY0_ON;
    curr_duty = 5;
    cycle_step = CYCLE_STEP;
    set_point = curr_duty;
  }
  
  else if ( keyInfo[1].state ) // set speed to 100 kph
  {
    next_state = KEY1_ON;
    curr_duty = 100;
    cycle_step = -CYCLE_STEP;
    set_point = curr_duty;
  }

  else if ( keyInfo[2].state ) // increase/decrease by 5 kph
  {
    next_state = KEY2_ON;
    if((cycle_step > 0 && curr_duty < 0xff) || (cycle_step < 0 && curr_duty >= CYCLE_STEP)){
      curr_duty += cycle_step;
      set_point = curr_duty;
    }

  }

  return next_state;
}

State currStateKey0On(KeyInfo *keyInfo)
{
  State next_state = KEY0_ON; 

  if ( !keyInfo[0].state )
  {
    next_state = NONE;
  }
  return next_state;
}

State currStateKey1On(KeyInfo *keyInfo)
{
  State next_state = KEY1_ON; 

  if ( !keyInfo[1].state )
  {
    next_state = NONE;
  }
  return next_state;
}

State currStateKey2On(KeyInfo *keyInfo)
{
  State next_state = KEY2_ON; 

  if ( !keyInfo[2].state )
  {
    next_state = NONE;
  }
  return next_state;
}

//
///////////////////////////////////////////////

ISR(TIMER0_OVF_vect)
{
  TCNT0 = 131;
  overflows++;
}

ISR(TIMER2_OVF_vect)
{
  OCR2B = curr_duty;
}

int main (void)
{
  // state machine lookup table
  // note: not required in submitted solutions, just showing how it's done
  getNextState keyProcessing[NUM_STATES] = {currStateNone, currStateKey0On, currStateKey1On, currStateKey2On};
  State curr_state = NONE;
  //unsigned char timestamp = 0;

  KeyInfo keyInfo[3] = { {OFF_MIN,false}, {OFF_MIN, false}, {OFF_MIN, false} };

  // Initialize port directions
  DDRF = 0x00;
  PORTF = 0x00;
  DDRB = 0xFF;
  PORTB = 0xFF;
  DDRH = 0xFF;
  PORTH = 0xFF;

  // initialize ADC to read the light sensor connected to ADC0 and use the internal 2.56V reference
  ADMUX = (1 << REFS1) | (1 << REFS0); //setup for using 2.56 internal voltage reference
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // setup 128 prescaler 

  set_sleep_mode( SLEEP_MODE_IDLE );
  
  sei();
  
  // start the timer for doing stuff
  reset_timer2();
  reset_timer0();
  
  // initialize the sample array for PWMing	
	for (int i=0; i<NUM_SAMPLE; i++){
		light[i] = 0;
	}

  // do an infinite loop
  for (;;) 
  {
    sleep_mode();
    
    if ( (overflows >= PROCESS_OVERFLOWS) )
    {
      update_ADC();
      pid_controller();	
      sample_inputs(keyInfo);
      
      // the core of the state machine is now a simple index into a table
      curr_state = keyProcessing[curr_state](keyInfo);
      
      overflows = 0;
    }
  }

  return 0;
}
