#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// timer 0 resolution -- conveniently set to overflow every 0.002048s with a 1MHz clock
#define TIMER0_SELECT  2

#define PROCESS_OVERFLOWS  50   // gives us .05s -- 20x/s

#define NUM_LEDS 4

// 300ms
#define BUTTON_DELAY (300/PROCESS_OVERFLOWS)
// 2 seconds
#define KEY_CLEAR    (2000/PROCESS_OVERFLOWS)
// a second
#define KEY_REPEAT   (1000/PROCESS_OVERFLOWS)

#define ON_LIMIT  2
#define ON_MAX    2
#define OFF_LIMIT 1
#define OFF_MIN   1

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
  KEY1_SINGLE_OFF,
  KEY1_DOUBLE_ON,
  KEY1_ON_HOLD,
  KEY1_OFF_HOLD,
  KEY2_ON,
  KEY2_OFF,
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

volatile unsigned int global_time = 0;

void reset_timer0()
{
  // set timer count rate and go

  // start at 131 to give us .001s per overflow (or 1ms)
  TCNT0 = 131;
  TCCR0B = TIMER0_SELECT;
  TIMSK0 |= _BV(TOIE0);
}

// let's do some hysteresis!
void sample_inputs()
{
  unsigned char keys = PIND;  // store now so we don't get changes as we go...
  unsigned char key;
  
  for ( key=PD0 ; key <= PD1 ; key++ )
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

// this is way too long but it does the job...
// I would not normally use a big switch statement for a state machine.
// This represents the minimum acceptable w.r.t. grading.
// Separate routines for state processing would be much better. As would a lookup table!
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
      if ( keyInfo[0].state )
      {
        curr_state = KEY1_SINGLE_ON;
        timestamp = 0;
      }
      
      else if ( keyInfo[1].state )
      {
        curr_state = KEY2_ON;
        timestamp = 0;
      }
      break;
      
    case KEY1_SINGLE_ON:
      timestamp++;
      if ( timestamp < BUTTON_DELAY && keyInfo[1].state )
      {
        leds[3] = !leds[3];
        curr_state = KEY2_KEY1_ON;
        timestamp = 0;
      }
        
      else if ( !keyInfo[0].state )
      {
        curr_state = KEY1_SINGLE_OFF;
        timestamp = 0;
      }
        
      else if ( timestamp > BUTTON_DELAY )
      {
        curr_state = KEY1_ON_HOLD;
      }
      break;
      
    case KEY1_SINGLE_OFF:
      timestamp++;
      if ( timestamp > BUTTON_DELAY )
      {
        leds[0] = !leds[0];
        curr_state = NONE;
      }
        
      else if ( keyInfo[0].state )
      {
        leds[2] = !leds[2];
        curr_state = KEY1_DOUBLE_ON;
        timestamp = 0;
      }
      break;
      
    case KEY1_DOUBLE_ON:
      timestamp++;
      if ( !keyInfo[0].state )
        curr_state = NONE;
      break;
      
    case KEY1_ON_HOLD:
      timestamp++;
      if ( timestamp >= KEY_CLEAR )
      {
        for ( i = 0; i<NUM_LEDS ; i++ )
          leds[i] = false;
        
        curr_state = KEY1_OFF_HOLD;
      }
        
      else if ( !keyInfo[0].state )
      {
        curr_state = KEY1_SINGLE_OFF;
        timestamp = BUTTON_DELAY;
      }
      break;
      
    case KEY1_OFF_HOLD:
      if ( !keyInfo[0].state )
      {
        curr_state = NONE;
      }
      break;
      
    case KEY2_ON:
      timestamp++;
      if ( timestamp < BUTTON_DELAY && keyInfo[0].state )
      {
        leds[3] = !leds[3];
        curr_state = KEY2_KEY1_ON;
        timestamp = 0;
      }
        
      else if ( !keyInfo[1].state )
      {
        curr_state = KEY2_OFF;
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
        leds[1] = !leds[1];
        timestamp = 0;
      }
        
      else if ( !keyInfo[1].state )
      {
        curr_state = NONE;
        timestamp = 0;
      }
      break;
      
    case KEY2_OFF:
      timestamp++;
      if ( timestamp > BUTTON_DELAY )
      {
        leds[1] = !leds[1];
        curr_state = NONE;
      }
      break;
      
    case KEY2_KEY1_ON:
      if ( !keyInfo[0].state && !keyInfo[1].state )
      {
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

int main (void)
{
  // Initialize port directions
  PORTB = 0xFF;
  DDRB = 0xFF;
  
  set_sleep_mode( SLEEP_MODE_IDLE );
  
  sei();

  reset_timer0();
  
  // do an infinite loop
  for (;;) 
  {
    sleep_mode();
    
    if ( (global_time >= PROCESS_OVERFLOWS) )
    {      
      sample_inputs();
      process_state();  
      global_time = 0;
    }
  }
}

ISR(TIMER0_OVF_vect)
{
  // start at 131 to give us .001s per overflow (or 1ms)
  TCNT0 = 131;
  global_time++;
}
