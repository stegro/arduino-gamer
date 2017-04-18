// =============================================================================
//
// Arduino - Tiny 3D Engine
// ------------------------
// by Themistokle "mrt-prodz" Benetatos
//
// This is a tiny 3D engine made for the ATMEGA328 and a Sainsmart 1.8" TFT screen (ST7735).
// It uses the amazingly fast Adafruit GFX library fork by XarkLabs. (github.com/XarkLabs)
//
// Features: - matrices for mesh transformations (rotation/translation/scaling)
//           - fixed point to avoid using floats
//           - 90 degrees fixed point look up table for sin/cos
//           - backface culling using shoelace algorithm
//           - flat colors (-unfinished/experimental/not fast enough-)
//           - rotate the mesh with a 3 axis accelerometer (ADXL335)
//           - rotate the mesh with a joystick thumb
//           - push button on digital PIN 2 to change the render type.
//
// Not implemented: - clipping
//                  - view/projection matrices (projection is done on world matrix directly)
//                  - no hidden surface removal
//
// You can use the python script stl2h.py to convert STL models into header files and
// automatically import your meshes, just include the file and comment other mesh headers.
//
// See stl2h.py help (./stl2h -h) for usage description.
//
// Developed and tested with an Arduino UNO and a Sainsmart 1.8" TFT screen.
//
// Dependencies:
// - https://github.com/XarkLabs/PDQ_GFX_Libs
//
// References:
// - http://www.tweaking4all.com/hardware/arduino/sainsmart-arduino-color-display
// - http://www.koonsolo.com/news/dewitters-gameloop
// - http://petercollingridge.appspot.com/3D-tutorial
// - http://forums.tigsource.com/index.php?topic=35880.0
// - http://www.mathopenref.com/coordpolygonarea.html
// - http://www.codinglabs.net/article_world_view_projection_matrix.aspx
//
// --------------------
// http://mrt-prodz.com
// http://github.com/mrt-prodz/ATmega328-Tiny-3D-Engine
//
// =============================================================================

#include <Adafruit_GFX.h>
#include "Gamer_SSD1306.h"
#include "fablablogo.h"

#include "type.h"
/* #include <SPI.h> */
/* #include <PDQ_GFX.h> */
/* #include "PDQ_ST7735_config.h" */
/* #include <PDQ_ST7735.h> */
/* PDQ_ST7735 display = PDQ_ST7735(); */

// ----------------------------------------------
// meshes
// ----------------------------------------------
// uncomment 1 header to automatically load mesh
//#include "mesh_cube.h"
#include "mesh_cone.h"
//#include "mesh_sphere.h"
//#include "mesh_torus.h"
//#include "mesh_monkey.h"


// define this, if your buttons get 'LOW' when you press them
#define BUTTON_ACTIVE_LOW

//Define Pins
#define BEEPER 6
#define BEEPER_GND 7
#define CONTROL_A A2
#define CONTROL_B A3

#define UNCONNECTED_PIN 0

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

Gamer_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);



// ----------------------------------------------
// defines
// ----------------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH/2)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT/2)

#define FOV              64

#define SKIP_TICKS       20.0                // 50fps
#define MAX_FRAMESKIP     5

#define COLOR0 BLACK
#define COLOR1 WHITE

#define NUMTYPES 2                           // number of render types
#define LUT(a) (long)(pgm_read_word(&lut[a]))// return value from LUT in PROGMEM

//#define USE_ACCELEROMETER                  // uncomment this to enable accelerometer
#ifdef USE_ACCELEROMETER
  #include "accelerometer.h"
#endif

//#define USE_JOYSTICK                       // uncomment this to enable joystick
#ifdef USE_JOYSTICK
  #include "joystick.h"
#endif


//#define DEBUG                              // uncomment this for debugging output to serial (leave disabled to save memory if needed)

//Matrix4 mMultiply(const Matrix4 &mat1, const Matrix4 &mat2) __attribute__((__optimize__("O2")));

// ----------------------------------------------
// global variables
// ----------------------------------------------
Matrix4 m_world;
Vector3i mesh_rotation = {0, 0, 0};
Vector3i mesh_position = {0, 0, 0};

const unsigned int lut[] PROGMEM = {         // 0 to 90 degrees fixed point COSINE look up table
  16384, 16381, 16374, 16361, 16344, 16321, 16294, 16261, 16224, 16182, 16135, 16082, 16025, 15964, 15897, 15825, 15749, 15668, 15582, 15491, 15395, 15295, 15190, 15081, 14967, 14848, 14725, 14598, 14466, 14329, 14188, 14043, 13894, 13740, 13582, 13420, 13254, 13084, 12910, 12732, 12550, 12365, 12175, 11982, 11785, 11585, 11381, 11173, 10963, 10748, 10531, 10310, 10086, 9860, 9630, 9397, 9161, 8923, 8682, 8438, 8191, 7943, 7691, 7438, 7182, 6924, 6663, 6401, 6137, 5871, 5603, 5334, 5062, 4790, 4516, 4240, 3963, 3685, 3406, 3126, 2845, 2563, 2280, 1996, 1712, 1427, 1142, 857, 571, 285, 0
};

static int proj_nodes[NODECOUNT][2];         // projected nodes (x,y)
static int old_nodes[NODECOUNT][2];          // projected nodes of previous frame to check if we need to redraw
static unsigned char i;
static int loops;
static double next_tick;
static double last_btn;                      // used for checking when the button was last pushed
static unsigned char draw_type = 1;          // 0 - vertex | 1 - wireframe | 2 - flat colors | ...

// ----------------------------------------------
// SIN/COS from 90 degrees LUT
// ----------------------------------------------
long SIN(unsigned int angle) {
  angle += 90;
  if (angle > 450) return LUT(0);
  if (angle > 360 && angle < 451) return -LUT(angle-360);
  if (angle > 270 && angle < 361) return -LUT(360-angle);
  if (angle > 180 && angle < 271) return  LUT(angle-180);
  return LUT(180-angle);
}

long COS(const unsigned int angle) {
  if (angle > 360) return LUT(0);
  if (angle > 270 && angle < 361) return  LUT(360-angle);
  if (angle > 180 && angle < 271) return -LUT(angle-180);
  if (angle > 90  && angle < 181) return -LUT(180-angle);
  return LUT(angle);
}

// ----------------------------------------------
// Matrix operation
// ----------------------------------------------
Matrix4 mMultiply(const Matrix4 &mat1, const Matrix4 &mat2) {
  Matrix4 mat;
  unsigned char r,c;
  for (c=0; c<4; c++)
    for (r=0; r<4; r++)
      mat.m[c][r] = pMultiply(mat1.m[0][r], mat2.m[c][0]) +
                    pMultiply(mat1.m[1][r], mat2.m[c][1]) +
                    pMultiply(mat1.m[2][r], mat2.m[c][2]) +
                    pMultiply(mat1.m[3][r], mat2.m[c][3]);
  return mat;
}

Matrix4 mRotateX(const unsigned int angle) {
  Matrix4 mat;
  mat.m[1][1] =  COS(angle);
  mat.m[1][2] =  SIN(angle);
  mat.m[2][1] = -SIN(angle);
  mat.m[2][2] =  COS(angle);
  return mat;
}

Matrix4 mRotateY(const unsigned int angle) {
  Matrix4 mat;
  mat.m[0][0] =  COS(angle);
  mat.m[0][2] = -SIN(angle);
  mat.m[2][0] =  SIN(angle);
  mat.m[2][2] =  COS(angle);
  return mat;
}

Matrix4 mRotateZ(const unsigned int angle) {
  Matrix4 mat;
  mat.m[0][0] =  COS(angle);
  mat.m[0][1] =  SIN(angle);
  mat.m[1][0] = -SIN(angle);
  mat.m[1][1] =  COS(angle);
  return mat;
}

Matrix4 mTranslate(const long x, const long y, const long z) {
  Matrix4 mat;
  mat.m[3][0] =  x << PSHIFT;
  mat.m[3][1] =  y << PSHIFT;
  mat.m[3][2] =  z << PSHIFT;
  return mat;
}

Matrix4 mScale(const float ratio) {
  Matrix4 mat;
  mat.m[0][0] *= ratio;
  mat.m[1][1] *= ratio;
  mat.m[2][2] *= ratio;
  return mat;
}

// ----------------------------------------------
// Shoelace algorithm to get the surface
// ----------------------------------------------
int shoelace(const int (*n)[2], const unsigned char index) {
  unsigned char t = 0;
  int surface = 0;
  for (; t<3; t++) {
    // (x1y2 - y1x2) + (x2y3 - y2x3) ...
    surface += (n[EDGE(index,t)][0]           * n[EDGE(index,(t<2?t+1:0))][1]) -
               (n[EDGE(index,(t<2?t+1:0))][0] * n[EDGE(index,t)][1]);
  }
  return surface * 0.5;
}

// ----------------------------------------------
// Shoelace algorithm for triangle visibility
// ----------------------------------------------
bool is_hidden(const int (*n)[2], const unsigned char index) {
  // (x1y2 - y1x2) + (x2y3 - y2x3) ...
  return ( ( (n[EDGE(index,0)][0] * n[EDGE(index,1)][1]) -
             (n[EDGE(index,1)][0] * n[EDGE(index,0)][1])   ) +
           ( (n[EDGE(index,1)][0] * n[EDGE(index,2)][1]) -
             (n[EDGE(index,2)][0] * n[EDGE(index,1)][1])   ) +
           ( (n[EDGE(index,2)][0] * n[EDGE(index,0)][1]) -
             (n[EDGE(index,0)][0] * n[EDGE(index,2)][1])   ) ) < 0 ? false : true;
}

// ----------------------------------------------
// draw projected nodes
// ----------------------------------------------
void draw_vertex(const int (*n)[2], const uint16_t color) {
  i = NODECOUNT-1;
  do {
    display.drawPixel(n[i][0],n[i][1], color);
  } while(i--);
}

// ----------------------------------------------
// draw edges between projected nodes
// ----------------------------------------------
void draw_wireframe(const int (*n)[2], const uint16_t color) {
  i = TRICOUNT-1;
  do {
    // don't draw triangle with negative surface value
    if (!is_hidden(n, i)) {
      // draw triangle edges - 0 -> 1 -> 2 -> 0
      /* display.drawLine(n[EDGE(i,0)][0], n[EDGE(i,0)][1], n[EDGE(i,1)][0], n[EDGE(i,1)][1], color); */
      /* display.drawLine(n[EDGE(i,1)][0], n[EDGE(i,1)][1], n[EDGE(i,2)][0], n[EDGE(i,2)][1], color); */
      /* display.drawLine(n[EDGE(i,2)][0], n[EDGE(i,2)][1], n[EDGE(i,0)][0], n[EDGE(i,0)][1], color); */

      display.drawTriangle(n[EDGE(i,0)][0], n[EDGE(i,0)][1],
			   n[EDGE(i,1)][0], n[EDGE(i,1)][1],
			   n[EDGE(i,2)][0], n[EDGE(i,2)][1], color);
    

      
    }
  } while(i--);
}


// ----------------------------------------------
// clear frame using bounding box (dirty mask)
// ----------------------------------------------
void clear_dirty(const int (*n)[2]) {
  unsigned char x0=SCREEN_WIDTH, y0=SCREEN_HEIGHT, x1=0, y1=0, c, w, h;
  // get bounding box of mesh
  for (c=0; c<NODECOUNT; c++) {
    if (n[c][0] < x0) x0 = n[c][0];
    if (n[c][0] > x1) x1 = n[c][0];
    if (n[c][1] < y0) y0 = n[c][1];
    if (n[c][1] > y1) y1 = n[c][1];
  }
  // clear area
  /* display.spi_begin(); */
  /* display.setAddrWindow_(x0, y0, x1, y1); */
  /* h = (y1-y0); */
  /* w = (x1-x0)+1; */
  /* do { */
  /*   display.spiWrite16(COLOR0, w); */
  /* } while (h--); */
  /* display.spi_end(); */
}

// ----------------------------------------------
// write current drawing mode in corner of screen
// ----------------------------------------------
void draw_print(const int16_t color) {
  display.setCursor(0, 2);
  display.setTextColor(color);
  display.setTextSize(0);
  switch(draw_type) {
    case 0: display.println(F(" vertex"));
            display.print(F(" count: "));
            display.print(NODECOUNT);
            break;
    default: display.println(F(" wireframe"));
            display.print(F(" triangles: "));
            display.println(TRICOUNT);
            break;
    /* case 2: display.println(F(" flat color")); */
    /*         display.println(F(" mask clear")); */
    /*         display.print(F(" triangles: ")); */
    /*         display.println(TRICOUNT); */
    /*         break; */
    /* case 3: display.println(F(" flat color")); */
    /*         display.println(F(" wireframe clear")); */
    /*         display.print(F(" triangles: ")); */
    /*         display.println(TRICOUNT); */
    /*         break; */
    /* case 4: display.println(F(" flat color")); */
    /*         display.println(F(" no clearscreen")); */
    /*         display.print(F(" triangles: ")); */
    /*         display.println(TRICOUNT); */
    /*         break; */
  }
}

// ----------------------------------------------
// setup
// ----------------------------------------------
void setup() {
  pinMode(BEEPER_GND, OUTPUT);
  digitalWrite(BEEPER_GND, LOW);

  display.begin(SSD1306_SWITCHCAPVCC);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV); // Set maximum diplay clock
  display.ssd1306_command(0xF0);                       
  display.clearDisplay();   // clears the screen and buffer
  display.display();   
  display.setTextWrap(false);

  splash();

  /* #ifdef DEBUG */
  /*   Serial.begin(57600); */
  /* #endif */
  /* // initialize the push button on pin 2 as an input */
  /* DDRD &= ~(1<<PD2); */
  /* // initialize screen */
  /* display.begin(); */
  /* display.fillScreen(COLOR0); */
  /* // print draw type on screen */
  /* draw_print(COLOR1); */
  /* // init tick */
  /* next_tick = last_btn = millis(); */
}
void splash()
{
  display.clearDisplay(); 

  display.drawBitmap(0, 0, logo, 128, 41, WHITE);

  display.fillRect(0,SCREEN_HEIGHT-10,SCREEN_WIDTH,10,WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(1,SCREEN_HEIGHT-9);
  display.print("Start: Poti bewegen");

  display.display();

  int controlA = analogRead(CONTROL_A);
  int controlB = analogRead(CONTROL_B);

  while (abs(controlA - analogRead(CONTROL_A) + controlB - analogRead(CONTROL_B)) < 10) {
    // show as long as the total absolute change of 
    // both potmeters is smaler than 5
  }

  soundArpeggioUp();

}

// ----------------------------------------------
// main loop
// ----------------------------------------------
void loop() {
  display.clearDisplay(); 
  loops = 0;
  while( millis() > next_tick && loops < MAX_FRAMESKIP) {
    // ===============
    // input
    // ===============
    // push button on digital PIN 2 to change render type
    /* if ( !(PIND & (1<<PD2)) && (millis()-last_btn) > 300 ) { */
    /*   // clear screen */
    /*   display.fillScreen(COLOR0); */
    /*   // change draw type */
    /*   draw_type++; */
    /*   if (draw_type > NUMTYPES) draw_type = 0; */
    /*   // print draw type on screen */
    /*   draw_print(COLOR1); */
    /*   // update last button push */
    /*   last_btn = millis(); */
    /* } */

    /* // rotation */
    m_world = mRotateX(mesh_rotation.x);
    /* m_world = mMultiply(mRotateY(mesh_rotation.y), m_world); */
    /* m_world = mMultiply(mRotateZ(mesh_rotation.z), m_world); */
    /* // scaling */
    m_world = mMultiply(mScale(1.5), m_world);

    // project nodes with world matrix
    Vector3i p;
    for (i=0; i<NODECOUNT; i++) {
      p.x = (m_world.m[0][0] * (NODE(i,0) >> PSHIFT)+
             m_world.m[1][0] * (NODE(i,1) >> PSHIFT) +
             m_world.m[2][0] * (NODE(i,2) >> PSHIFT) +
             m_world.m[3][0]) / PRES;
      
      p.y = (m_world.m[0][1] * (NODE(i,0) >> PSHIFT) +
             m_world.m[1][1] * (NODE(i,1) >> PSHIFT) +
             m_world.m[2][1] * (NODE(i,2) >> PSHIFT) +
             m_world.m[3][1]) / PRES;
            
      p.z = (m_world.m[0][2] * (NODE(i,0) >> PSHIFT) +
             m_world.m[1][2] * (NODE(i,1) >> PSHIFT) +
             m_world.m[2][2] * (NODE(i,2) >> PSHIFT) +
             m_world.m[3][2]) / PRES;

      // store projected node
      proj_nodes[i][0] = (FOV * p.x) / (FOV + p.z) + HALF_SCREEN_WIDTH;
      proj_nodes[i][1] = (FOV * p.y) / (FOV + p.z) + HALF_SCREEN_HEIGHT;
    }

    #ifdef USE_ACCELEROMETER
      // if accelerometer is specified use it to rotate the mesh instead of the default rotation mode
      mesh_rotation.x = accel_get_value(&accel.index.x, accel.readings.x, &accel.total.x, ACCEL_XPIN);
      mesh_rotation.y = accel_get_value(&accel.index.y, accel.readings.y, &accel.total.y, ACCEL_YPIN);
      mesh_rotation.z = accel_get_value(&accel.index.z, accel.readings.z, &accel.total.z, ACCEL_ZPIN);
    #elif defined USE_JOYSTICK
      // if joystick is specified use it to rotate the mesh instead of the default rotation mode
      mesh_rotation.x = joystick_get_value(&joystick.index.x, joystick.readings.x, &joystick.total.x, JOYSTICK_XPIN);
      mesh_rotation.y = joystick_get_value(&joystick.index.y, joystick.readings.y, &joystick.total.y, JOYSTICK_YPIN);
    #else
      // default auto-rotation mode
      mesh_rotation.x+=3;
      mesh_rotation.y+=2;
      mesh_rotation.z++;
    #endif

    if (mesh_rotation.x > 360) mesh_rotation.x = 0;
    if (mesh_rotation.y > 360) mesh_rotation.y = 0;
    if (mesh_rotation.z > 360) mesh_rotation.z = 0;
    // ...
    next_tick += SKIP_TICKS;
    loops++;
  }    

  // ===============
  // draw
  // ===============
  // only redraw if nodes position have changed (less redraw - less flicker)
  if (memcmp(old_nodes, proj_nodes, sizeof(proj_nodes))) {
    // render frame
    switch(draw_type) {
      case 0: draw_vertex(old_nodes, COLOR0);
              draw_vertex(proj_nodes, COLOR1);
              break;
      default: if (TRICOUNT > 32) clear_dirty(old_nodes);
              else draw_wireframe(old_nodes, COLOR0);
              draw_wireframe(proj_nodes, COLOR1);
              break;
    }
    // copy projected nodes to old_nodes to check if we need to redraw next frame
    memcpy(old_nodes, proj_nodes, sizeof(proj_nodes));
  }
  display.display();
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
