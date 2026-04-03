#ifndef KEYMAP_H
#define KEYMAP_H

#if defined(__arm__) && defined(__linux__)
#define _BTN_KB_ESCAPE    -1
#define _BTN_KB_ENTER     -1
#define _BTN_KB_BACKSPACE -1
#define _BTN_KB_DELETE    -1
#define _BTN_KB_TAB       -1
#define _BTN_KB_SHIFT     -1
#define _BTN_KB_CTRL      -1
#define _BTN_KB_ALT       -1
#define _BTN_KB_ALT2      -1
#define _BTN_KB_FN        -1
#define _BTN_KB_META      -1
#define _BTN_KB_CAPS_LOCK -1
#define _BTN_KB_PG_UP     -1
#define _BTN_KB_PG_DOWN   -1
#define _BTN_KB_HOME      -1
#define _BTN_KB_END       -1
#define _BTN_KB_INSERT    -1
#define _BTN_KB_UP        -1
#define _BTN_KB_DOWN      -1
#define _BTN_KB_LEFT      -1
#define _BTN_KB_RIGHT     -1
#define _BTN_KB_SPACE     -1
#define _BTN_GP_UP        119
#define _BTN_GP_DOWN      115
#define _BTN_GP_LEFT      113
#define _BTN_GP_RIGHT     100
#define _BTN_GP_A         97
#define _BTN_GP_B         98
#define _BTN_GP_X         120
#define _BTN_GP_Y         121
#define _BTN_GP_L1        104
#define _BTN_GP_R1        108
#define _BTN_GP_L2        106
#define _BTN_GP_R2        107
#define _BTN_GP_SELECT    110
#define _BTN_GP_START     109
#define _BTN_GP_MENU      117
#define _BTN_GP_VOL_UP    114
#define _BTN_GP_VOL_DOWN  116
#define _BTN_GP_POWER_ESC 0
#else
#if defined(GFX_SDL)
#define _BTN_KB_ESCAPE    27
#define _BTN_KB_ENTER     13
#define _BTN_KB_BACKSPACE 8
#define _BTN_KB_DELETE    127
#define _BTN_KB_TAB       9
#define _BTN_KB_SHIFT     48
#define _BTN_KB_CTRL      50
#define _BTN_KB_ALT       54
#define _BTN_KB_ALT2      53
#define _BTN_KB_FN        53
#define _BTN_KB_META      52
#define _BTN_KB_CAPS_LOCK 45
#define _BTN_KB_PG_UP     24
#define _BTN_KB_PG_DOWN   25
#define _BTN_KB_HOME      22
#define _BTN_KB_END       23
#define _BTN_KB_INSERT    -1
#define _BTN_KB_UP        17
#define _BTN_KB_DOWN      18
#define _BTN_KB_LEFT      20
#define _BTN_KB_RIGHT     19
#define _BTN_KB_SPACE     32
#elif defined(GFX_SDL2)
#define _BTN_KB_ESCAPE    27
#define _BTN_KB_ENTER     13
#define _BTN_KB_BACKSPACE 127
#define _BTN_KB_DELETE    40
#define _BTN_KB_TAB       9
#define _BTN_KB_SHIFT     -31
#define _BTN_KB_CTRL      -32
#define _BTN_KB_ALT       -29
#define _BTN_KB_ALT2      -25
#define _BTN_KB_FN        -25
#define _BTN_KB_META      -30
#define _BTN_KB_CAPS_LOCK 57
#define _BTN_KB_PG_UP     75
#define _BTN_KB_PG_DOWN   78
#define _BTN_KB_HOME      74
#define _BTN_KB_END       77
#define _BTN_KB_INSERT    -1
#define _BTN_KB_UP        82
#define _BTN_KB_DOWN      81
#define _BTN_KB_LEFT      80
#define _BTN_KB_RIGHT     79
#define _BTN_KB_SPACE     32
#elif defined(GFX_MACOS) && defined(__APPLE__)
#define _BTN_KB_ESCAPE    27
#define _BTN_KB_ENTER     13
#define _BTN_KB_BACKSPACE 127
#define _BTN_KB_DELETE    40
#define _BTN_KB_TAB       9
#define _BTN_KB_SHIFT     -1
#define _BTN_KB_CTRL      -1
#define _BTN_KB_ALT       -1
#define _BTN_KB_ALT2      -1
#define _BTN_KB_FN        -1
#define _BTN_KB_META      -1
#define _BTN_KB_CAPS_LOCK -1
#define _BTN_KB_PG_UP     44
#define _BTN_KB_PG_DOWN   45
#define _BTN_KB_HOME      41
#define _BTN_KB_END       43
#define _BTN_KB_INSERT    -1
#define _BTN_KB_UP        0
#define _BTN_KB_DOWN      1
#define _BTN_KB_LEFT      2
#define _BTN_KB_RIGHT     3
#define _BTN_KB_SPACE     32
#elif defined(GFX_BUFFER) || !(defined(GFX_SDL) || defined(GFX_SDL2))
#define _BTN_KB_ESCAPE    -3
#define _BTN_KB_ENTER     -4
#define _BTN_KB_BACKSPACE -5
#define _BTN_KB_DELETE    -6
#define _BTN_KB_TAB       -7
#define _BTN_KB_SHIFT     -8
#define _BTN_KB_CTRL      -9
#define _BTN_KB_ALT       -10
#define _BTN_KB_ALT2      -11
#define _BTN_KB_FN        -12
#define _BTN_KB_META      -13
#define _BTN_KB_CAPS_LOCK -14
#define _BTN_KB_PG_UP     -15
#define _BTN_KB_PG_DOWN   -16
#define _BTN_KB_HOME      -17
#define _BTN_KB_END       -18
#define _BTN_KB_INSERT    -19
#define _BTN_KB_UP        -20
#define _BTN_KB_DOWN      -21
#define _BTN_KB_LEFT      -22
#define _BTN_KB_RIGHT     -23
#define _BTN_KB_SPACE     -24
#endif

#ifndef GP_OVERRIDE
#define _BTN_GP_UP        'w'
#define _BTN_GP_DOWN      's'
#define _BTN_GP_LEFT      'a'
#define _BTN_GP_RIGHT     'd'
#define _BTN_GP_A         'l'
#define _BTN_GP_B         'k'
#define _BTN_GP_X         'i'
#define _BTN_GP_Y         'j'
#define _BTN_GP_L1        '1'
#define _BTN_GP_R1        '0'
#define _BTN_GP_L2        '2'
#define _BTN_GP_R2        '9'
#define _BTN_GP_SELECT    _BTN_KB_SPACE
#define _BTN_GP_START     _BTN_KB_ENTER
#define _BTN_GP_MENU      _BTN_KB_TAB
#define _BTN_GP_VOL_UP    _BTN_KB_PG_UP
#define _BTN_GP_VOL_DOWN  _BTN_KB_PG_DOWN
#define _BTN_GP_POWER_ESC _BTN_KB_END
#endif
#endif

#define BTN_KB_ESCAPE     ((char)_BTN_KB_ESCAPE)
#define BTN_KB_ENTER      ((char)_BTN_KB_ENTER)
#define BTN_KB_BACKSPACE  ((char)_BTN_KB_BACKSPACE)
#define BTN_KB_DELETE     ((char)_BTN_KB_DELETE)
#define BTN_KB_TAB        ((char)_BTN_KB_TAB)
#define BTN_KB_SHIFT      ((char)_BTN_KB_SHIFT)
#define BTN_KB_CTRL       ((char)_BTN_KB_CTRL)
#define BTN_KB_ALT        ((char)_BTN_KB_ALT)
#define BTN_KB_ALT2       ((char)_BTN_KB_ALT2)
#define BTN_KB_FN         ((char)_BTN_KB_FN)
#define BTN_KB_META       ((char)_BTN_KB_META)
#define BTN_KB_CAPS_LOCK  ((char)_BTN_KB_CAPS_LOCK)
#define BTN_KB_PG_UP      ((char)_BTN_KB_PG_UP)
#define BTN_KB_PG_DOWN    ((char)_BTN_KB_PG_DOWN)
#define BTN_KB_HOME       ((char)_BTN_KB_HOME)
#define BTN_KB_END        ((char)_BTN_KB_END)
#define BTN_KB_INSERT     ((char)_BTN_KB_INSERT)
#define BTN_KB_UP         ((char)_BTN_KB_UP)
#define BTN_KB_DOWN       ((char)_BTN_KB_DOWN)
#define BTN_KB_LEFT       ((char)_BTN_KB_LEFT)
#define BTN_KB_RIGHT      ((char)_BTN_KB_RIGHT)
#define BTN_KB_SPACE      ((char)_BTN_KB_SPACE)
#define BTN_GP_UP         ((char)_BTN_GP_UP)
#define BTN_GP_DOWN       ((char)_BTN_GP_DOWN)
#define BTN_GP_LEFT       ((char)_BTN_GP_LEFT)
#define BTN_GP_RIGHT      ((char)_BTN_GP_RIGHT)
#define BTN_GP_A          ((char)_BTN_GP_A)
#define BTN_GP_B          ((char)_BTN_GP_B)
#define BTN_GP_X          ((char)_BTN_GP_X)
#define BTN_GP_Y          ((char)_BTN_GP_Y)
#define BTN_GP_L1         ((char)_BTN_GP_L1)
#define BTN_GP_R1         ((char)_BTN_GP_R1)
#define BTN_GP_L2         ((char)_BTN_GP_L2)
#define BTN_GP_R2         ((char)_BTN_GP_R2)
#define BTN_GP_SELECT     ((char)_BTN_GP_SELECT)
#define BTN_GP_START      ((char)_BTN_GP_START)
#define BTN_GP_MENU       ((char)_BTN_GP_MENU)
#define BTN_GP_VOL_UP     ((char)_BTN_GP_VOL_UP)
#define BTN_GP_VOL_DOWN   ((char)_BTN_GP_VOL_DOWN)
#define BTN_GP_POWER_ESC  ((char)_BTN_GP_POWER_ESC)

typedef struct {
  int key;
  const char * name;
} keyinfo_t;

extern keyinfo_t keymap[];

#endif