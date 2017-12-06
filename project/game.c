/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6

static char score[2];
int wait;
int delay = 0;

AbRect rect10 = {abRectGetBounds, abRectCheck, {15,5}}; // rectangle that will serve as a paddle

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
}; 
  

Layer paddle1 = {		/**< layer for a paddle */
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight/8 - 5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_VIOLET,
  0
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &paddle1
};

Layer paddle2 = {		/**< Layer with a second paddle */
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight - 16}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &fieldLayer,
};

Layer ballLayer = {		/**< Layer with the ball */
  (AbShape *)&circle4,
  {(screenWidth/2), (screenHeight/2)}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_ORANGE,
  &paddle2,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer ml2 = { &paddle1, {0,0}, 0 }; /**< not all layers move */
MovLayer ml1 = { &paddle2, {0,0}, &ml2 }; 
MovLayer ml0 = { &ballLayer, {2,1},  &ml1 }; 

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  


/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}


/* method to check collisions between the ball and a paddle */
void collCheck(MovLayer *ball, Region *p1, Region *p2) {
    Vec2 newPos;
    u_char axis;
    Region ballBoundary;
    
    vec2Add(&newPos, &ball->layer->posNext, &ball->velocity);
    abShapeGetBounds(ball->layer->abShape, &newPos, &ballBoundary);
    if (((ballBoundary.topLeft.axes[1] <= p1->botRight.axes[1]) &&
        (ballBoundary.topLeft.axes[0] > p1->topLeft.axes[0]) &&
        (ballBoundary.topLeft.axes[0] < p1->botRight.axes[0])) ||
        ((ballBoundary.botRight.axes[1] >= p2->topLeft.axes[1]) &&
        (ballBoundary.botRight.axes[0] > p2->topLeft.axes[0]) &&
        (ballBoundary.botRight.axes[0] < p2->botRight.axes[0]))) {
    int velocity = ball->velocity.axes[1] = -ball->velocity.axes[1];
    newPos.axes[1] += (2*velocity);
    }
}

void scoreCheck(MovLayer *ml, Region *fence) {      /* checks to see if a player has scored or not */
    
    Vec2 newPos;
    u_char axis;
    Region shapeBoundary;
    
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    
    if (shapeBoundary.topLeft.axes[1] < fence->topLeft.axes[1]) {
        changeScore(&score, 2);
        delay = 1;
    }
    
    if (shapeBoundary.botRight.axes[1] > fence->botRight.axes[1]) {
        changeScore(&score, 1);
    }
}

void changeScore(char *i, int player) {     /* changes the score for each player */
    if (player == 1) {
        i[0]++;
    }
    if (player == 2) {
        i[1]++;
    }
}

void resetBall(MovLayer *ml) {      /* resests the board after a player scores */
    
    Vec2 newPos;
    u_char axis;
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    int velocity = -ml->velocity.axes[0];
    newPos.axes[0] = (screenWidth/2);
    newPos.axes[1] = (screenHeight/2);
    ml->layer->posNext = newPos;
}

/* functions to handle miving the paddles left and right */
void movPaddleRight (MovLayer *ml) {
    Vec2 newPos;
    u_char axis;
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    int velocity = ml->velocity.axes[0];
    newPos.axes[0] += (2+velocity);
    ml->layer->posNext = newPos;
}

void movPaddleLeft (MovLayer *ml) {
    Vec2 newPos;
    u_char axis;
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    int velocity = ml->velocity.axes[0];
    newPos.axes[0] -= (2+velocity);
    ml->layer->posNext = newPos;
}


u_int bgColor = COLOR_BLUE;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */
Region p1;
Region p2;

  

/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  score[0] = '0';
  score[1] = '0';
  
  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);

  shapeInit();

  layerInit(&ballLayer);
  layerDraw(&ballLayer);


  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */


  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml0, &ballLayer);
    layerGetBounds(&paddle1, &p1);
    layerGetBounds(&paddle2, &p2);
    drawString5x7(5,0, "score: ", COLOR_GREEN, COLOR_BLACK);
    drawString5x7(5,150, "score: ", COLOR_GREEN, COLOR_BLACK);
    drawChar5x7(45,0, score[0], COLOR_GREEN, COLOR_BLACK);
    drawChar5x7(45,150, score[1], COLOR_GREEN, COLOR_BLACK);
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
      
      if (delay == 1) {
          // resetBall(&ml0);
          // layerDraw(&ballLayer);
          while(++wait < 10000) {
              // wait ++;
          }
          resetBall(&ml0);
          while(++wait < 30000) {}
          delay = 0;
          wait = 0;
      }
      else {
        collCheck(&ml0, &p1, &p2);
        mlAdvance(&ml0, &fieldFence);
        scoreCheck(&ml0, &fieldFence);
        count = 0;
      }
  
  /* sets the switches and reads their input */
    u_int switches = p2sw_read(), w;
  char str[5];
  
  for (w = 0; w < 4; w++)
      str[w] = (switches & (1<<w)) ? '-' : '0'+w;
  str[4] = 0;
  
  if (str[0] == '0') {
      movPaddleLeft(&ml1);
  }
  
  if (str[1] == '1') {
      movPaddleRight(&ml1);
  }
  
  if (str[2] == '2') {
      movPaddleLeft(&ml2);
  }
  
  if (str[3] == '3') {
      movPaddleRight(&ml2);
  }
    redrawScreen = 1;
    count = 0;
  } 
  
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
