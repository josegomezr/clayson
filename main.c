// Must be defined in one file, _before_ #include "clay.h"
#define CLAY_IMPLEMENTATION

#include <ctype.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define bool int


// #define CLAY__FLOATEQUAL_FN(x, y) x == y
// #define CLAY__FLOAT int32_t
// #define CLAY__MAXFLOAT INT32_MAX 
// #define CLAY_DISABLE_SIMD 1
// #define CLAY__EPSILON 0

#include "clay.h"
#include "clay_renderer_terminal.c"

const Clay_Color COLOR_BLACK = (Clay_Color) {0, 0, 0, 255};
const Clay_Color COLOR_WHITE = (Clay_Color) {255, 255, 255, 255};
const Clay_Color COLOR_RED = (Clay_Color) {255, 0, 0, 255};
const Clay_Color COLOR_GREEN = (Clay_Color) {0, 127, 0, 255};
const Clay_Color COLOR_BLUE = (Clay_Color) {0, 0, 255, 255};
const Clay_Color COLOR_LIGHT = (Clay_Color) {100, 100, 100, 255};

static uint64_t frameCount = 0;
static float renderSpeed = 0;
int mX = 0;
int mY = 0;
int mP = 0;
Clay_Dimensions wsize = {
  .width = (CLAY__FLOAT) 80,
  .height = (CLAY__FLOAT) 24,
};

Clay_String txt_clicked = CLAY_STRING("CLICKED");
Clay_String txt_hovered = CLAY_STRING("HOVERED");
Clay_String txt_nope = CLAY_STRING("NOPE");
Clay_String txt_btn = CLAY_STRING("NOPE");

static Clay_Color highlight_color = (Clay_Color) {255, 0, 0, 255};

// // An example function to begin the "root" of your layout tree
int button1 = 0;
int button2 = 0;
int button3 = 0;

void HandleButtonInteraction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
  int *buttonData = (int *)userData;
    // Pointer state allows you to detect mouse down / hold / release
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
        *buttonData = 1;

    // if( pointerInfo.state == CLAY_POINTER_DATA_RELEASED)
    //     *buttonData = 0;

    if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME){
         *buttonData = 0;
      // HERE DO THINGS
    }

}

void RenderButton(Clay_String txt, int* buttonState){
  CLAY({
      .layout = {
      .padding = CLAY_PADDING_ALL(1),
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = {
          .width = CLAY_SIZING_FIXED(10),
          .height = CLAY_SIZING_FIXED(3)
        }
      },
      .backgroundColor = Clay_Hovered() ? (*buttonState ? COLOR_RED : COLOR_BLUE) : COLOR_GREEN,
      .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = COLOR_LIGHT }
    }) {
      Clay_OnHover(HandleButtonInteraction, (intptr_t)buttonState);
      CLAY_TEXT(Clay_Hovered() ? (*buttonState ? txt_clicked : txt_hovered) : txt, CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE }));
    } 
}

Clay_RenderCommandArray CreateLayout() {
  float startTime = (CLAY__FLOAT)clock()/CLOCKS_PER_SEC;
  frameCount++;
  // highlight_color = COLOR_RED;
  Clay_BeginLayout();

  CLAY({
    .id = CLAY_ID("OuterContainer"),
    .layout = {
      .layoutDirection = CLAY_LEFT_TO_RIGHT,
      .sizing = {
        CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
      },
    },
  }) {
    CLAY({
        .id = CLAY_ID("SideBar"),
        .layout = {
          .padding = CLAY_PADDING_ALL(1),
          .layoutDirection = CLAY_TOP_TO_BOTTOM,
          .sizing = {
            .width = CLAY_SIZING_FIXED(20),
            .height = CLAY_SIZING_PERCENT(1)
          }
        },
        .backgroundColor = COLOR_RED,
        .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = COLOR_WHITE }
    }) {
      CLAY_TEXT(CLAY_STRING("Sidebar"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
      RenderButton(CLAY_STRING("B1"), &button1);
      RenderButton(CLAY_STRING("B2"), &button2);
      RenderButton(CLAY_STRING("B3"), &button3);
    }
    
    CLAY({
      .id = CLAY_ID("OtherSideBar"),
        .layout = {
        .padding = CLAY_PADDING_ALL(1),
          .layoutDirection = CLAY_TOP_TO_BOTTOM,
          .sizing = {
            .width = CLAY_SIZING_GROW(),
            .height = CLAY_SIZING_PERCENT(1)
          }
        },
        .backgroundColor = COLOR_LIGHT
    }) {
      CLAY_TEXT(CLAY_STRING("Sidebar"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
      char result[128];
      sprintf(result, "MX: %d MY: %d MC: %d F:%d fps: %.5f", mX, mY, mP, frameCount, renderSpeed > 0 ? 1/renderSpeed : 0);
      Clay_String txt = (Clay_String) {
        .length = strlen(result),
        .chars = result
      };
      
      // TODO font size is wrong, only one is allowed, but I don't know which it is
      CLAY_TEXT(
        txt,
          CLAY_TEXT_CONFIG({
            .wrapMode = CLAY_TEXT_WRAP_WORDS,
            .fontId = 1,
            .fontSize = 1,
            .textColor = COLOR_GREEN
          })
      );
    }
  }
  renderSpeed = ((CLAY__FLOAT)clock()/CLOCKS_PER_SEC) - startTime;

  return Clay_EndLayout();
}


void HandleClayErrors(Clay_ErrorData errorData) {
    // See the Clay_ErrorData struct for more information
    printf("ERR-%d: %s", errorData.errorType, errorData.errorText.chars);
}

void cook() {
  printf("\x1B[?1049l"); // Disable alternative buffer.
  printf("\x1B[?1000l"); // Disable alternative buffer.
  printf("\x1B[?47l"); // Restore screen.
  printf("\x1B[u"); // Restore cursor position.
  printf("\x1B[?25h"); // Hide the cursor.
}

void uncook() {
  printf("\x1B[?25l"); // show the cursor.
  printf("\x1B[s"); // Save cursor position.
  printf("\x1B[?47h"); // Save screen.
  printf("\x1B[?1000h"); // Disable alternative buffer.
  printf("\x1B[?1049h"); // save cursor and enable alternative buffer.
}

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  cook();
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VTIME] = 8;
  raw.c_cc[VMIN] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  uncook();
}

void setTermSize(Clay_Dimensions* wsize) {
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
      return;
    wsize->width = (CLAY__FLOAT) w.ws_col;
    wsize->height = (CLAY__FLOAT) w.ws_row;
}

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

enum editorKey {
  ARROW_LEFT = 0x100,
  ARROW_RIGHT = 0x201,
  ARROW_UP = 0x302,
  ARROW_DOWN = 0x303
};

char rawRead() {
  char c;
  int nread = read(STDIN_FILENO, &c, 1);
  if (nread == -1 && errno != EAGAIN) die("read");
  return c;
}

int editorReadKey() {
  char seq[10] = {0};
  read(STDIN_FILENO, seq, 10);
  char *ptr = seq;

  if (*ptr == '\x1b') {
    ptr += 1;
    if (*ptr == '[') {
      int ctrl = 0;

      reparse_char:
      switch (ptr[1]) {
        case '1': {
          if (ctrl == 0){
            ptr = ptr+3;
            ctrl = 0x1f;
            goto reparse_char;
          }
        };
        case 'A': return (ARROW_UP + ctrl);
        case 'B': return (ARROW_DOWN + ctrl);
        case 'C': return (ARROW_RIGHT + ctrl);
        case 'D': return (ARROW_LEFT + ctrl);
        case 'M': {
          // only left click, i'm lazy.
          mP = (ptr[2] == 0x20);
          // -1 is important, we use 0-based indexing, the shell uses 1-based indexing
          mX = ptr[3] - 32 - 1;
          mY = ptr[4] - 32 - 1;
        }
      }
    }
    return '\x1b';
  } else {
    return *ptr;
  }
}

#define CTRL_KEY(k) ((k) & 0x1f)

void editorProcessKeypress() {
  int c = editorReadKey();
  // Console_MoveCursor(10, 10);
  // printf("%x:%c", c, c);
  // fflush(stdout);
  // sleep(1);
  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    // case ' ':
    case '\r':
      mP = !mP;
      break;
    case ARROW_UP:
      mY = CLAY__MAX(0, mY - 1);
      break;
    case ARROW_DOWN:
      mY = CLAY__MIN(wsize.height, mY + 1);
      break;
    case ARROW_LEFT:
      mX = CLAY__MAX(0, mX - 1);
      break;
    case ARROW_RIGHT:
      mX = CLAY__MIN(wsize.width, mX + 1);
      break;
    case (ARROW_LEFT+0x1f):
      mX = CLAY__MAX(0, mX - 10);
      break;
    case (ARROW_UP+0x1f):
      mY = CLAY__MAX(0, mY - 10);
      break;
    case (ARROW_DOWN+0x1f):
      mY = CLAY__MIN(wsize.height, mY + 5);
      break;
    case (ARROW_RIGHT+0x1f):
      mX = CLAY__MIN(wsize.width, mX + 10);
      break;
  }
}

void DrawFrame() {
  Clay_RenderCommandArray layout = CreateLayout();
  Clay_Console_Render(layout, wsize.width, wsize.height);
  fflush(stdout);
}


int main() {
  enableRawMode();
  setTermSize(&wsize);

  uint64_t totalMemorySize = Clay_MinMemorySize();
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(
    arena,
    wsize,
    (Clay_ErrorHandler) {
      HandleClayErrors
    }
  );

  // TODO this is wrong, but I have no idea what the actual size of the terminal is in pixels
  // // Tell clay how to measure text
  Clay_SetMeasureTextFunction(Console_MeasureText, NULL);
  while (1){
    Clay_SetPointerState((Clay_Vector2) { mX, mY }, mP);
    DrawFrame();
    editorProcessKeypress();
  }
}