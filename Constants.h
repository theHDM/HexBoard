// hardware pins
#define SDAPIN 16
#define SCLPIN 17
#define LED_PIN 22
#define ROT_PIN_A 20
#define ROT_PIN_B 21
#define ROT_PIN_C 24
#define MPLEX_1_PIN 4
#define MPLEX_2_PIN 5
#define MPLEX_4_PIN 2
#define MPLEX_8_PIN 3
#define COLUMN_PIN_0 6
#define COLUMN_PIN_1 7
#define COLUMN_PIN_2 8
#define COLUMN_PIN_3 9
#define COLUMN_PIN_4 10
#define COLUMN_PIN_5 11
#define COLUMN_PIN_6 12
#define COLUMN_PIN_7 13
#define COLUMN_PIN_8 14
#define COLUMN_PIN_9 15
#define TONEPIN 23

// grid related
#define LED_COUNT 140
#define COLCOUNT 10
#define ROWCOUNT 14

#define HEX_DIRECTION_EAST 0
#define HEX_DIRECTION_NE   1
#define HEX_DIRECTION_NW   2
#define HEX_DIRECTION_WEST 3
#define HEX_DIRECTION_SW   4
#define HEX_DIRECTION_SE   5

#define CMDBTN_0 0
#define CMDBTN_1 20
#define CMDBTN_2 40
#define CMDBTN_3 60
#define CMDBTN_4 80
#define CMDBTN_5 100
#define CMDBTN_6 120
#define CMDCOUNT 7

// microtonal related
#define TUNINGCOUNT 13

#define TUNING_12EDO 0
#define TUNING_17EDO 1
#define TUNING_19EDO 2
#define TUNING_22EDO 3
#define TUNING_24EDO 4
#define TUNING_31EDO 5
#define TUNING_41EDO 6
#define TUNING_53EDO 7
#define TUNING_72EDO 8
#define TUNING_BP 9
#define TUNING_ALPHA 10
#define TUNING_BETA 11
#define TUNING_GAMMA 12     

#define MAX_SCALE_DIVISIONS 72
#define ALL_TUNINGS 255

// MIDI-related
#define CONCERT_A_HZ 440.0
#define PITCH_BEND_SEMIS 2
#define CMDB 192
#define UNUSED_NOTE 255
#define CC_MSG_COOLDOWN_MICROSECONDS 32768

// buzzer related
#define TONE_SL 3
#define TONE_CH 1
#define WAVEFORM_SQUARE 0
#define WAVEFORM_SAW 1
#define POLL_INTERVAL_IN_MICROSECONDS 32
#define POLYPHONY_LIMIT 15
#define ALARM_NUM 2
#define ALARM_IRQ TIMER_IRQ_2
#define BUZZ_OFF 0
#define BUZZ_MONO 1
#define BUZZ_ARPEGGIO 2
#define BUZZ_POLY 3

// LED related

// value / brightness ranges from 0..255
// black = 0, full strength = 255

#define VALUE_BLACK 0
#define VALUE_LOW   64
#define VALUE_SHADE 128
#define VALUE_NORMAL 192
#define VALUE_FULL  255

// saturation ranges from 0..255
// 0 = black and white
// 255 = full chroma

#define SAT_BW 0
#define SAT_TINT 32
#define SAT_DULL 85
#define SAT_MODERATE 170
#define SAT_VIVID 255

// hue is an angle from 0.0 to 359.9
// there is a transform function to map "perceptual"
// hues to RGB. the hue values below are perceptual.
#define HUE_NONE 0.0
#define HUE_RED 0.0
#define HUE_ORANGE 36.0
#define HUE_YELLOW 72.0
#define HUE_LIME 108.0
#define HUE_GREEN 144.0
#define HUE_CYAN 180.0
#define HUE_BLUE 216.0
#define HUE_INDIGO 252.0
#define HUE_PURPLE 288.0
#define HUE_MAGENTA 324.0

#define RAINBOW_MODE 0
#define TIERED_COLOR_MODE 1

// animations
#define ANIMATE_NONE 0
#define ANIMATE_STAR 1 
#define ANIMATE_SPLASH 2 
#define ANIMATE_ORBIT 3 
#define ANIMATE_OCTAVE 4 
#define ANIMATE_BY_NOTE 5

// menu-related
#define MENU_ITEM_HEIGHT 10
#define MENU_PAGE_SCREEN_TOP_OFFSET 10
#define MENU_VALUES_LEFT_OFFSET 78

// debug
#define DIAGNOSTIC_OFF 0
#define DIAGNOSTIC_ON 1