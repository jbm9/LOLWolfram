#include <Charliplexing.h>
#include <Myfont.h>


#define DISPLAYHEIGHT 9
typedef uint16_t state_entry_t;
#define STATE_ENTRY_SIZE 8*sizeof(state_entry_t)

#define STATE_WIDTH 10
 // 20*16=320bits, which allows for appx 300 steps before the visible
 // state is potentially invalid

struct LOLWolfram {
  uint8_t pattern;
  uint8_t cursor;

  uint16_t stepno;

  uint16_t lines[DISPLAYHEIGHT];

  state_entry_t curstate[STATE_WIDTH];
};


LOLWolfram lulz;

#define SERIALDEBUG 0
#define DISPDEBUG 0

#define STATE_ENTRY_BIT(se, i) ( (se) & (1<<(i)) )
#define FULL_STATE_ENTRY_BIT(state, i) ( STATE_ENTRY_BIT((state)[(i)/STATE_ENTRY_SIZE], (i)%STATE_ENTRY_SIZE) )
#define FULL_STATE_ENTRY_TOGGLE(state, i) ( (state)[(i)/STATE_ENTRY_SIZE] ^= (1 << ((i)%STATE_ENTRY_SIZE) ) )

uint16_t LOLWolfram_step(struct LOLWolfram *lw) {
  // We're going to update the curstate variable in-place

  // We're going to combine the old left bit, old current bit, and old
  // right bit into the mask for the new current bit.  This is a
  // left-to-right operation, so we can pull the right bit out of the
  // current values, and the left bits out of the last values.
  //    0 1 0 0 0 0 1 1 0 1 1 0 ...
  //     \|/ \|/
  //    0 1 0 0 ...


  
  // Given stepno, we know where in the buffer our valid data starts: 

  uint16_t centerbit = (STATE_ENTRY_SIZE) * (STATE_WIDTH)/2 + (STATE_ENTRY_SIZE - 1);
  uint16_t leftmostbit = centerbit - lw->stepno; // this is the "leading zero" of the last step

#if SERIALDEBUG
  /*  Serial.print("center bit=");
  Serial.println(centerbit);
  Serial.print("leftmost bit=");
  Serial.println(leftmostbit);*/


  for (uint16_t i = 0; i < STATE_ENTRY_SIZE*STATE_WIDTH; i++) {
    Serial.print( FULL_STATE_ENTRY_BIT(lw->curstate, i) ? "*" : " ");
  }
  Serial.println();

#endif

    //   - cache the current state entry
  state_entry_t entry_current = lw->curstate[ (leftmostbit/STATE_ENTRY_SIZE) ];

  // for the leftmost bit to the rightmost bit, we need to:
  for (uint16_t i = 0; i < (lw->stepno)*2+1; i++) {

    uint16_t curbit = leftmostbit + i;

    uint8_t bitcursor = curbit % STATE_ENTRY_SIZE;

    if (1 == bitcursor) { // fully stepped into the next entry
      entry_current = lw->curstate[ curbit/STATE_ENTRY_SIZE ];
    }

    //   - fetch the left bit out of the above
    //   - fetch the current and right bits out of the current state
    uint8_t next_bit_rule = 
      4*STATE_ENTRY_BIT(entry_current, (bitcursor-1)%STATE_ENTRY_SIZE) +
      2*FULL_STATE_ENTRY_BIT(lw->curstate, curbit) +
      1*FULL_STATE_ENTRY_BIT(lw->curstate, curbit+1);

    //   - compute the new value    
    uint8_t next_bit_value = (lw->pattern & (1<<next_bit_rule)) ? 1 : 0;

    //   - store it in the current bit
    if (FULL_STATE_ENTRY_BIT(lw->curstate, curbit) != next_bit_value)
      FULL_STATE_ENTRY_TOGGLE(lw->curstate, curbit);
  }

  lw->stepno++;
  //  - copy the relevant state into the output buffer

  uint16_t l = 0x0000;
  for (uint8_t i = 0; i < 16; i++) {
    l |= (FULL_STATE_ENTRY_BIT(lw->curstate, centerbit-8+i)) << i;
  }
  lw->lines[lw->cursor] = l;
  lw->cursor = (lw->cursor+1) % DISPLAYHEIGHT;

#if 0

  uint16_t l = lw->lines[lw->cursor];

  uint16_t retval = 0x00;

#if SERIALDEBUG
  Serial.print("Step. cursor=");
  Serial.println((int)lw->cursor);
#endif

  for (int i = 1; i <= 15; i++) {
    int j = (l & (7 << (i-1))) >> (i-1);

    if (lw->pattern & (1<<j))
      retval |= 1<<i;
  }

  // Enforce the top of the triangle
  if (lw->stepno <= 7) {
    uint16_t presentmask = ((2 << (2*lw->stepno))-1) << (7-lw->stepno);

    retval = retval & presentmask;
  }

  if (lw->stepno < 20) {
    lw->stepno++;
  }



  lw->cursor = (lw->cursor + 1) % DISPLAYHEIGHT;
  lw->lines[lw->cursor] = retval;

#if SERIALDEBUG
  Serial.print("Exit step. cursor=");
  Serial.println((int)lw->cursor);
#endif
  return retval;
#endif
}

void LOLWolfram_display(struct LOLWolfram *l) {
  for (int dy = 0; dy < DISPLAYHEIGHT; dy++) {
    uint16_t curline = l->lines[ (l->cursor+1+dy)%DISPLAYHEIGHT ];

    for (int x = 1; x < 15; x++) {
      uint8_t b = (curline & (1 << x)) ? 1 : 0;

#if !SERIALDEBUG
      LedSign::Set(x-1,DISPLAYHEIGHT-1-dy, b); // (l->cursor+dy)%DISPLAYHEIGHT
#else
      //  Serial.print(b ? "*" : " ");
#endif

    }
#if SERIALDEBUG
    //    Serial.println();
#endif

  }


#if !SERIALDEBUG && DISPDEBUG
  for (int x = 0; x < 2; x++) {
    for (int y = 0; y < 5; y++) {
      LedSign::Set(x, y, 0);

      if (x == 0 & 0 != (l->stepno & (1<<y))) {
	LedSign::Set(0, y, 1);
      }
    }
  }
#endif
}

void LOLWolfram_init(struct LOLWolfram *lulz, uint8_t pattern) {
  memset(lulz, 0, sizeof(struct LOLWolfram));
  lulz->pattern = pattern;
  lulz->cursor = 0;
  lulz->lines[0] = 1 << 7;
  lulz->stepno = 1; // we did the 0th step above
  memset(lulz->curstate, 0, STATE_WIDTH*STATE_ENTRY_SIZE/8);
  FULL_STATE_ENTRY_TOGGLE(lulz->curstate, (STATE_ENTRY_SIZE)*(1+((STATE_WIDTH)/2))-1);
}



#define MODE_INTERESTING 0
#define MODE_COMPLETE 1
uint8_t mode = MODE_COMPLETE;



void setup() {
  LOLWolfram_init(&lulz, 137);

#if SERIALDEBUG
  Serial.begin(57600);
  Serial.println("Starting up.");

  Serial.println("LED Initialization skipped due to conflicts.");
#else
  LedSign::Init();
  showPatternNo(mode);
  delay(1000);
  LOLWolfram_display(&lulz);
#endif
}

void showPatternNo(uint8_t i) {
  uint8_t x = 0;

  char q[3] = "XX";
  sprintf(q, "%02x", i);

  for (int j = 0; j < 3; j++) {
    Myfont::Draw(x, q[j]);
    x += 6;
  }
}

uint8_t patterns[12] = { 255, 137, 30, 18, 
			 70, 102, 105, 118,
			 193, 195, 255, 0 };

uint8_t curpattern = 0;

void loop() {
  LOLWolfram_step(&lulz);
  LOLWolfram_display(&lulz);

  uint16_t speedno = analogRead(4);
  delay(100+speedno);


  uint16_t x = 12*analogRead(5)/1024;

  if (x != curpattern) {
    if (mode == MODE_INTERESTING) {
      curpattern = x;
      LOLWolfram_init(&lulz, patterns[curpattern]);

    } else if (mode == MODE_COMPLETE) {
      uint8_t is_skip = ((curpattern + 1 == x) || (curpattern - 1 == x)) ? 1 : 0;
      
      uint8_t is_down = (curpattern > x) ? 1 : 0;

      uint8_t dir = (is_down ^ is_skip) ? -1 : 1;

      curpattern = x;
      LOLWolfram_init(&lulz, (lulz.pattern+dir)%256);
    }

#if !SERIALDEBUG
    LedSign::Clear();
    showPatternNo(lulz.pattern);
    delay(300);
#else
    Serial.println(lulz.pattern);
#endif


  }
}
