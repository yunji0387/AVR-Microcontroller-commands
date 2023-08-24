#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// timer 0 resolution -- conveniently set to overflow every 0.002048s with a 1MHz clock
#define TIMER0_SELECT  2

#define PROCESS_OVERFLOWS  50   // gives us .05s -- 20x/s

#define NUM_LEDS 4

// 300ms
#define BUTTON_DELAY (300/PROCESS_OVERFLOWS)
// half second
#define KEY_REPEAT   (350/PROCESS_OVERFLOWS)

#define ON_LIMIT  2
#define ON_MAX    2
#define OFF_LIMIT 1
#define OFF_MIN   1

#define LENGTH_8_BIT 255
#define INITIAL_SPEED 30 // this is the initial fan speed, is also the minimum speed the fan required to start spinning
#define MIN_SPEED 0 // minimum fan speed
#define MAX_SPEED 100 // maximum fan speed
#define DUTY_CYCYLE_SCALE 10 // scale for increase/decrease duty cycle everytime

//uint8_t interrupt_counter = 0; //  counter uses to count how many interrupt occurred 
double duty_cycle = INITIAL_SPEED;  

enum BOOL
{
  false,
  true
};
typedef enum BOOL boolean;

enum STATES
{
  NONE,
  KEY1_SINGLE_ON,
  KEY1_ON_HOLD,
  KEY2_ON,
  KEY2_ON_HOLD,
  KEY2_KEY1_ON,
  NUM_STATES
};

typedef enum STATES State;

struct KEY_INFO
{
  unsigned int count;
  boolean      state;
};

typedef struct KEY_INFO KeyInfo;

static KeyInfo keyInfo[2] = { {OFF_MIN,false}, {OFF_MIN, false} };

volatile unsigned int overflows = 0;

void reset_timer0()
{
  // set timer count rate and go

  // start at 131 to give us .001s per overflow (or 1ms)
  TCNT0 = 131;
  TCCR0B = TIMER0_SELECT;
  TIMSK0 |= _BV(TOIE0);
}

//Hysteresis (mostly from solution)
void sample_inputs()
{
  unsigned char keys = PIND;  // store now so we don't get changes as we go...
  unsigned char key;
  
  for ( key=PD1 ; key <= PD2 ; key++ )
  {
    if ( (keys & _BV(key)) == 0 )
    {
      if ( keyInfo[key-1].count < ON_MAX )
        keyInfo[key-1].count++;
    }
    else if ( keyInfo[key-1].count > OFF_MIN )
      keyInfo[key-1].count--;
    
    if ( keyInfo[key-1].count >= ON_LIMIT )
      keyInfo[key-1].state = true;
    else if ( keyInfo[key-1].count <= OFF_LIMIT )
      keyInfo[key-1].state = false;
  }
}

// increase/decrease the duty cycle based on parameter
void rpm_controller(boolean isIncrease)
{
    uint8_t duty_cycle_scale = DUTY_CYCYLE_SCALE;
    if(!isIncrease && duty_cycle >= DUTY_CYCYLE_SCALE)
    {
        duty_cycle -= duty_cycle_scale;
    }
    else if(isIncrease && duty_cycle <= (MAX_SPEED - DUTY_CYCYLE_SCALE))
    {
        if(duty_cycle == MIN_SPEED) //if fan is off, we set the fan to INITIAL_SPEED so that it can start spinning
        {
            duty_cycle_scale = INITIAL_SPEED;
        }
        duty_cycle += duty_cycle_scale;
    } 
}

//State Machine (mostly from solution)
void process_state()
{
  static State curr_state = NONE;
  static boolean leds[NUM_LEDS];
  // each run is 50ms so we can use that to our advantage here...
  static unsigned char timestamp = 0;
  int i;
  
  switch ( curr_state )
  {
    case NONE:
      for ( i = 0; i<NUM_LEDS ; i++ )
        leds[i] = false;
      if ( keyInfo[0].state )
      {
        leds[1] = !leds[1];
        curr_state = KEY1_SINGLE_ON;
        timestamp = 0;
      }
      
      else if ( keyInfo[1].state )
      {
        leds[2] = !leds[2];
        curr_state = KEY2_ON;
        timestamp = 0;
      }
      break;
      
    case KEY1_SINGLE_ON:
      timestamp++;
      if ( timestamp < BUTTON_DELAY && keyInfo[1].state )
      {
        // when both switches pressed we don't change speed
        leds[3] = !leds[3];
        leds[2] = !leds[2];
        curr_state = KEY2_KEY1_ON;
        timestamp = 0;
      }
        
      else if ( !keyInfo[0].state )
      {
        curr_state = NONE;
        rpm_controller(false); //decrease fan speed
        timestamp = 0;
      }
        
      else if ( timestamp > BUTTON_DELAY )
      {
        curr_state = KEY1_ON_HOLD;
      }
      break;
      
    case KEY1_ON_HOLD:
      timestamp++;
      if ( timestamp >= KEY_REPEAT )
      {
        leds[1] = !leds[1];
        rpm_controller(false); //decrease fan speed
        timestamp = 0;
      }

      else if ( !keyInfo[0].state )
      {
        curr_state = NONE;
        timestamp = 0;
      }
      break;
      
    case KEY2_ON:
      timestamp++;
      if ( timestamp < BUTTON_DELAY && keyInfo[0].state )
      {
        // when both switches pressed we don't change speed
        leds[1] = !leds[1];
        leds[3] = !leds[3];
        curr_state = KEY2_KEY1_ON;
        timestamp = 0;
      }
        
      else if ( !keyInfo[1].state )
      {
        curr_state = NONE;
        rpm_controller(true); //increase fan speed
        timestamp = 0;
      }
        
      else if ( timestamp > KEY_REPEAT )
      {
        curr_state = KEY2_ON_HOLD;
      }      
      break;
      
    case KEY2_ON_HOLD:
      timestamp++;
      if ( timestamp >= KEY_REPEAT )
      {
        leds[2] = !leds[2];
        rpm_controller(true); //increase fan speed
        timestamp = 0;
      }
        
      else if ( !keyInfo[1].state )
      {
        curr_state = NONE;
        timestamp = 0;
      }
      break;
      
    case KEY2_KEY1_ON:
      if ( !keyInfo[0].state && !keyInfo[1].state )
      {
        // when both switches pressed we don't change speed
        curr_state = NONE;
      }
      break;
      
    default:
      break;
      
  } 

  for ( i=0 ; i<NUM_LEDS ; i++ )
  {
    if ( leds[i] )
      PORTB &= ~_BV(i);
    else
      PORTB |= _BV(i);
  }
}

//initial ports,timers setup
void init_setup()
{
    // Initialize port directions
    PORTB = 0xFF; // initially off all leds
    DDRB = 0xFF;  //set portb as ouput

    DDRH = (1 << PORTH6); //set PH6 as output

    //setup for timer 2 using Fast PWM mode (non-inverting)
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20); // Set timer 2 to use CTC mode (non-inverting mode)
    TCCR2B = (1 << CS21); // Set timer 2 to use a 8 prescaler
    OCR2B = (duty_cycle/MAX_SPEED)*LENGTH_8_BIT; // Set timer 2 compare match scale of duty cycle ticks
    TIMSK2 = (1 << TOIE2); // Enable timer 2 overflow interrupt

    set_sleep_mode( SLEEP_MODE_IDLE );
    
    sei(); // enable interrupt

    reset_timer0();
}

int main (void)
{
    init_setup();
    // do an infinite loop
    for (;;) 
    {
        sleep_mode();
        
        if ( (overflows >= PROCESS_OVERFLOWS) )
        {      
            sample_inputs();
            process_state();
        
            overflows = 0;
        }
    }
}

ISR(TIMER0_OVF_vect)    //interrupt for heartbeat
{
    // start at 131 to give us .001s per overflow (or 1ms)
    TCNT0 = 131;
    overflows++;
}

ISR(TIMER2_OVF_vect)    //interrupt for duty cycle
{  
    OCR2B = (duty_cycle/MAX_SPEED)*LENGTH_8_BIT; //update the dutycyle scale
}