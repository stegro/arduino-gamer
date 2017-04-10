/*************************************************** 
  This is a Pong port for the Arduino. The application 
  uses an Arduino Uno, Adafruit’s 128x64 OLED display, 
  2 potentiometers and an piezo buzzer.

  More info about this project can be found on my blog:
  http://michaelteeuw.nl

  Written by Michael Teeuw | Xonay Labs.  
  Apache 2 license, all text above must be included 
  in any redistribution.
 ****************************************************/

#include <Adafruit_GFX.h>
#include "Gamer_SSD1306.h"
#include "fablablogo.h"

//#define SHOW_DEBUG_DATA

// define this, if your buttons get 'LOW' when you press them
#define BUTTON_ACTIVE_LOW

//Define Pins
#define BEEPER 6
#define BEEPER_GND 7
#define CONTROL_A A2
#define CONTROL_B A3


// Note that according to https://www.arduino.cc/en/Reference/attachInterrupt
// only buttons A and B can have interrupts
#define PIN_BUTTON_A 2
#define PIN_BUTTON_B 3
#define PIN_BUTTON_C 4
#define PIN_BUTTON_D 5
const int debounceDelay = 100;

#define OLED_CLK   13
#define OLED_MOSI  11  // Hardware SPI port
#define OLED_RESET 12
#define OLED_DC    10
#define OLED_CS     9

//Define Visuals
#define FONT_SIZE 1
#define SCREEN_WIDTH 127  //real size minus 1, because coordinate system starts with 0
#define SCREEN_HEIGHT 63  //real size minus 1, because coordinate system starts with 0
#define PADDLE_WIDTH 4
#define PADDLE_HEIGHT 14
#define PADDLE_PADDING 10
#define BALL_SIZE 3
#define SCORE_PADDING 10

#define EFFECT_SPEED 0.8
#define MAX_Y_SPEED 2


//Define Variables
Gamer_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

const byte UNDEF_STATE = 0;
const byte MENU_STATE = 1;
const byte PLAY_STATE = 2;

byte game_state = UNDEF_STATE;

byte settings = 0;
byte menu_cursor = 0;
const char* settings_text[] = {"Sound","Spiel beschleunigt"};
const byte SETTING_SOUND = 1<<0;
const byte SETTING_ACCEL = 1<<1;
const byte n_settings = 2;

int paddleLocationA = 0;
int paddleLocationB = 0;

float ballX = SCREEN_WIDTH/2;
float ballY = SCREEN_HEIGHT/2;
// if SETTING_ACCEL is active, these parameters determine game acceleration 
float ballSpeedXDelta = 0.1;
float ballSpeedXStartWithAccel = 1.2;

float ballSpeedXStartWithoutAccel = 2;

/*
the ballSpeed consists of
  0) a regular speed (which justs persists; according to game level, hits, etc),
  1) an additive effect contribution which vanishes at the next paddle hit
  2) an additive effect contribution which vanishes at the next wall hit
*/
float ballSpeedX[] = {ballSpeedXStartWithoutAccel, 0.0, 0.0};
float ballSpeedY[] = {1.0, 0.0, 0.0};
/*
the ballSpeedFactor is applied to the total speed, and consists of
  0) a regular factor (which just persists)
  1) a factor which becomes 1.0 at the next paddle hit
  1) a factor which becomes 1.0 at the next wall hit
*/
float ballSpeedFactor[] = {1.0, 1.0, 1.0};

float totalBallSpeedX = 0;
float totalBallSpeedY = 0;

int lastPaddleLocationA = 0;
int lastPaddleLocationB = 0;

int scoreA = 0;
int scoreB = 0;
int hitCounter = 0;

void setup() 
{
  pinMode(BEEPER_GND, OUTPUT);
  digitalWrite(BEEPER_GND, LOW);
  
  display.begin(SSD1306_SWITCHCAPVCC);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV); // Set maximum diplay clock
  display.ssd1306_command(0xF0);                       
  display.clearDisplay();   // clears the screen and buffer
  display.display();   
  display.setTextWrap(false);

  splash();
  
  display.setTextColor(WHITE);
  display.setTextSize(FONT_SIZE);
  display.clearDisplay();

  game_state = MENU_STATE;

#ifdef BUTTON_ACTIVE_LOW
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_A), menu_cursor_down, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_B), menu_toggle, FALLING);
#else
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_A), menu_cursor_down, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_B), menu_toggle, RISING);
#endif

}

//Splash Screen
void splash()
{
  display.clearDisplay(); 

  display.drawBitmap(0, 0, logo, 128, 41, WHITE);

  display.fillRect(0,SCREEN_HEIGHT-10,SCREEN_WIDTH,10,WHITE);
  display.setTextColor(BLACK);
  centerPrint("Start: Poti bewegen",SCREEN_HEIGHT-9,1);

  display.display();

  int controlA = analogRead(CONTROL_A);
  int controlB = analogRead(CONTROL_B);

  while (abs(controlA - analogRead(CONTROL_A) + controlB - analogRead(CONTROL_B)) < 10) {
    // show as long as the total absolute change of 
    // both potmeters is smaler than 5
  }

  soundArpeggioUp();
}

/*
  display a menu, to set handicap etc.
*/
void settings_menu()
{
  byte line_height = 1 + 7 + 1;
  display.clearDisplay(); 
  display.setTextColor(WHITE);
  centerPrint("EINSTELLUNGEN",1,1);

  char onoff_char[] = "x";
  for(byte i = 0; i < n_settings; i++) {
    display.setTextSize(1);
    //display.setCursor(1,y);
    if(settings & ( 1 << i )) {
      if(i == menu_cursor){
	onoff_char[0] = 'X';
      }else{
	onoff_char[0] = 'x';
      }
    }else{
      if(i == menu_cursor){
	onoff_char[0] = 'O';
      } else {
	onoff_char[0] = 'o';
      }
    }
    lalignPrint(onoff_char,1,(1+i)*line_height);
    lalignPrint(settings_text[i],9,(1+i)*line_height);
  }

  display.fillRect(0,SCREEN_HEIGHT-10,SCREEN_WIDTH,10,WHITE);
  display.setTextColor(BLACK);
  scrollPrint("Abwaerts: Taste A  Aendern: B  Start: D  ",SCREEN_HEIGHT-9,1);

  if(digitalRead(PIN_BUTTON_D) == LOW){
    // do some final initialisation according to chosen settings
    if(settings & SETTING_ACCEL) { 
      ballSpeedX[0] = ballSpeedXStartWithAccel;
    } else {
      ballSpeedX[0] = ballSpeedXStartWithoutAccel;
    }
    
    // leave menu and start playing
    game_state = PLAY_STATE;
  }

  display.display();
}

void menu_cursor_down() {
  //If an object that has static storage duration is not initialized explicitly, then:
  // if it has arithmetic type, it is initialized to (positive or unsigned) zero;
  static int last_time_pressed;
  
  if(game_state = MENU_STATE && millis() - last_time_pressed > debounceDelay) {
    menu_cursor = (menu_cursor + 1) % n_settings;
    last_time_pressed = millis();
  }
}

void menu_toggle() {
  // If an object that has static storage duration is not initialized
  // explicitly, then: if it has arithmetic type, it is initialized to
  // (positive or unsigned) zero
  static int last_time_pressed;
  
  if(game_state = MENU_STATE && millis() - last_time_pressed > debounceDelay) {
    // toggle the setting the menu_cursor points to
    settings = settings ^ (1 << menu_cursor);
    last_time_pressed = millis();
  }
}

void loop()
{
  if(game_state == MENU_STATE) {
    settings_menu();
  } else if(game_state == PLAY_STATE) {
    play_loop();
  }
}

void play_loop()
{
  calculateMovement();
  draw();
}

void calculateMovement() 
{
  int controlA = analogRead(CONTROL_A);
  int controlB = analogRead(CONTROL_B);

  paddleLocationA = map(controlA, 0, 1023, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  paddleLocationB = map(controlB, 0, 1023, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);

  int paddleSpeedA = paddleLocationA - lastPaddleLocationA;
  int paddleSpeedB = paddleLocationB - lastPaddleLocationB;

  ballX += totalBallSpeedX;
  ballY += totalBallSpeedY;

  //bounce from top and bottom
  if (ballY >= SCREEN_HEIGHT - BALL_SIZE || ballY <= 0) {
    // clear effects
    ballSpeedX[2] = 0.0;
    ballSpeedFactor[2] = 1.0;

    reverseVelocity(ballSpeedY,3);
    soundBounce();
  }

  //bounce from paddle A. The ballSpeed is used to avoid artifacts.
  if (totalBallSpeedX < 0 && ballTouchesRect(ballX, ballY, BALL_SIZE,
		      PADDLE_PADDING, paddleLocationA,
		      PADDLE_WIDTH, PADDLE_HEIGHT)) {
    //clear effects
    ballSpeedX[1] = 0.0;
    ballSpeedFactor[1] = 1.0;

    reverseVelocity(ballSpeedX,3);
    
    hitCounter++;
    addPaddleSpeedEffect(paddleSpeedA);
    if(settings & SETTING_ACCEL && hitCounter % 10 == 0) {
      ballSpeedFactor[0] += 0.05;
      soundArpeggioUp();
    } else
      soundBounce();
  }

  if (totalBallSpeedX > 0 && ballTouchesRect(ballX, ballY, BALL_SIZE,
		      SCREEN_WIDTH-PADDLE_PADDING-PADDLE_WIDTH, paddleLocationB,
		      PADDLE_WIDTH, PADDLE_HEIGHT)) {
    //clear effects
    ballSpeedX[1] = 0.0;
    ballSpeedFactor[1] = 1.0;
    
    reverseVelocity(ballSpeedX,3);

    hitCounter++;
    addPaddleSpeedEffect(paddleSpeedB);

    if(settings & SETTING_ACCEL && hitCounter % 10 == 0) {
      ballSpeedFactor[0] += 0.05;
      soundArpeggioUp();
    } else
      soundBounce();
  }

  //score points if ball hits wall behind paddle
  if (ballX >= SCREEN_WIDTH - BALL_SIZE) {
    scoreA++;
    soundPoint();

    // put Ball back in game
    ballX = SCREEN_WIDTH / 4 * 3;
    reverseVelocity(ballSpeedX,3);
    
  } else if(ballX <= 0) {
    scoreB++;
    soundPoint();

    ballX = SCREEN_WIDTH / 4;
    reverseVelocity(ballSpeedX,3);

  }

  //set last paddle locations
  lastPaddleLocationA = paddleLocationA;
  lastPaddleLocationB = paddleLocationB;

  // accelerate x-motion of the ball, if option is activated
  totalBallSpeedX = 0.0;
  totalBallSpeedY = 0.0;
  for(byte i = 0; i < 3; i++) {
    totalBallSpeedX += ballSpeedX[i];
    totalBallSpeedY += ballSpeedY[i];
  }
  for(byte i = 0; i < 3; i++) {
    totalBallSpeedX *= ballSpeedFactor[i];
    totalBallSpeedY *= ballSpeedFactor[i];
  }
}

void draw() 
{
  display.clearDisplay(); 

  //draw paddle A
  display.fillRect(PADDLE_PADDING,paddleLocationA,PADDLE_WIDTH,PADDLE_HEIGHT,WHITE);

  //draw paddle B
  display.fillRect(SCREEN_WIDTH-PADDLE_WIDTH-PADDLE_PADDING,paddleLocationB,PADDLE_WIDTH,PADDLE_HEIGHT,WHITE);

  //draw center line
  for (byte i=0; i<SCREEN_HEIGHT; i+=4) {
    display.drawFastVLine(SCREEN_WIDTH/2, i, 2, WHITE);
  }

  //draw ball
  display.fillRect(ballX,ballY,BALL_SIZE,BALL_SIZE,WHITE);

  //print scores
  display.setTextColor(WHITE);

  //backwards indent score A
  byte scoreAWidth = 6 * FONT_SIZE * ceil(log10(scoreA));

  display.setCursor(SCREEN_WIDTH/2 - SCORE_PADDING - scoreAWidth,0);
  display.print(scoreA);

  //+1 because of dotted line
  display.setCursor(SCREEN_WIDTH/2 + SCORE_PADDING+1,0);
  display.print(scoreB);

#ifdef SHOW_DEBUG_DATA
  //display some debug data
  display.setCursor(0,0);
  display.print(hitCounter);
  display.setCursor(0,9);
  display.print(totalBallSpeedX);
  display.setCursor(0,9*2);
  display.print(totalBallSpeedY);
#endif
  
  display.display();
} 

/*

 */
void addPaddleSpeedEffect(int paddleSpeed)
{
  //add effect to ball when paddle is moving while bouncing.
  //for every pixel of paddle movement, add or substact EFFECT_SPEED to ballspeed.
  ballSpeedY[0] += floor(paddleSpeed)*EFFECT_SPEED;

  //limit to maximum speed
  ballSpeedY[0] = signum(ballSpeedY[0]) * min(abs(ballSpeedY[0]),MAX_Y_SPEED);
}

void soundArpeggioUp() 
{
  tone(BEEPER, 250);
  delay(100);
  tone(BEEPER, 500);
  delay(100);
  tone(BEEPER, 1000);
  delay(100);
  noTone(BEEPER);
}

/*
note that this returns 1 (true) already on equality.
 */
byte ballTouchesRect(byte pointX, byte pointY, byte size, byte rectUpLeftX, byte rectUpLeftY, byte rectSizeX, byte rectSizeY)
{
  // the ball (actually a rectangle) and the paddle area intersect
  return rectUpLeftX <= pointX + size &&
  	  rectUpLeftY <= pointY + size &&
  	  pointX <= rectUpLeftX + rectSizeX &&
  	  pointY <= rectUpLeftY + rectSizeY;

  // the ball (actually a rectangle) and the paddle front line intersect
  /* return rectUpLeftX <= pointX + size && */
  /* 	  rectUpLeftY <= pointY + size && */
  /* 	  pointX <= rectUpLeftX + 1 && */
  /* 	  pointY <= rectUpLeftY + 1; */
  
}

void reverseVelocity(float *contributions, byte n)
{
  for(byte i = 0; i < n; i++)
    contributions[i] *= -1;
}

/*
this returns -1, 0 or +1, depending on the sign of value
 */
signed char signum(float value)
{
  // a branchless way to express signum
  return (0 < value) - (value < 0);
}

void soundBounce() 
{
  if(settings & SETTING_SOUND)
    tone(BEEPER, 500, 50);
}

void soundPoint() 
{
  if(settings & SETTING_SOUND)
    tone(BEEPER, 150, 150);
}

void centerPrint(const char *text, byte y, byte size)
{
  display.setTextSize(size);
  display.setCursor(SCREEN_WIDTH/2 - ((strlen(text))*6*size)/2,y);
  display.print(text);
}

void scrollPrint(const char *text, byte y, byte size)
{
  //If an object that has static storage duration is not initialized explicitly, then:
  // if it has arithmetic type, it is initialized to (positive or unsigned) zero;
  static int xposition;

  int text_length = (strlen(text))*6*size;
  xposition -= 2;
  xposition = xposition < -text_length ? xposition + text_length : xposition;

  display.setTextSize(size);

  display.setCursor(xposition,y);
  display.print(text);

  if(xposition + text_length < SCREEN_WIDTH) {
    display.setCursor(xposition+text_length,y);
    display.print(text);
  }

}

void lalignPrint(const char *text, byte x, byte y)
{
  display.setTextSize(1);
  display.setCursor(x,y);
  display.print(text);
}
