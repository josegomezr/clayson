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
#include "cjson.c"
#include <stdio.h>

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
int mouseX = 0;
int mouseY = 0;
int clickActive = 0;

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

#define MAX_DEPTH 5

typedef struct {
  // These need to be freed
  cJSON* json;
  char* main_screen_buffer;

  // These are weak references
  cJSON* currentNode;
  cJSON* parent;
  cJSON* stack[MAX_DEPTH];
  size_t curridx;
} ApplicationState;

static ApplicationState appstate = {
  .main_screen_buffer = NULL,
  .json = NULL,
  .currentNode = NULL,
  .parent = NULL,
  .stack = {0},
  .curridx = 0
};


void HandleButtonInteraction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
  cJSON * node = (cJSON *)userData;

  // On hover, show the currently selected key
  appstate.currentNode = node;

  // When clicked: 
  if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME){
    if (appstate.stack[appstate.curridx] == node && appstate.curridx > 0)
    {
      appstate.stack[appstate.curridx] = NULL;
      appstate.curridx = CLAY__MAX(0, appstate.curridx-1);
      return;
    }
    if (cJSON_IsObject(node) || cJSON_IsArray(node)){
        appstate.stack[++appstate.curridx] = appstate.currentNode;
        appstate.currentNode = node;
    }
  }

  if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME){
  }
}

void RenderKeyLabel(Clay_String txt, cJSON* node){
  CLAY({
      .layout = {
      .padding = CLAY_PADDING_ALL(0),
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = {
          .width = CLAY_SIZING_FIXED(16),
          .height = CLAY_SIZING_FIT(1)
        }
      },
      .backgroundColor = Clay_Hovered() ? (appstate.currentNode == node ? COLOR_RED : COLOR_BLUE) : COLOR_GREEN,
      .border = { .width = CLAY_BORDER_OUTSIDE(0), .color = COLOR_LIGHT }
    }) {
      Clay_OnHover(HandleButtonInteraction, (intptr_t)node);
      CLAY_TEXT(txt, CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE }));
    } 
}

Clay_RenderCommandArray CreateLayout(ApplicationState* appstate) {
  float startTime = (CLAY__FLOAT)clock()/CLOCKS_PER_SEC;
  frameCount++;
  Clay_BeginLayout();

  CLAY({
    .id = CLAY_ID("MainWindow"),
    .layout = {
      .layoutDirection = CLAY_LEFT_TO_RIGHT,
      .sizing = {
        CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
      },
    },
  }) {
    CLAY({
        .id = CLAY_ID("Keys"),
        .layout = {
          .padding = CLAY_PADDING_ALL(1),
          .layoutDirection = CLAY_TOP_TO_BOTTOM,
          .sizing = {
            .width = CLAY_SIZING_FIXED(20),
            .height = CLAY_SIZING_PERCENT(1)
          }
        },
        // .backgroundColor = COLOR_RED,
        .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = COLOR_WHITE }
    }) {
      CLAY_TEXT(CLAY_STRING("Keys"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
      
      Clay_String gototop_label = CLAY_STRING("~ ROOT ~");
      if (appstate->curridx > 0)
      {
        gototop_label = (Clay_String) {
          .length = appstate->stack[appstate->curridx]->string_len,
          .chars = appstate->stack[appstate->curridx]->string
        };
      }

      RenderKeyLabel(gototop_label, appstate->stack[appstate->curridx]);
      int i = 0;
      for (struct cJSON* ptr = (appstate->stack[appstate->curridx])->child; ptr != NULL; ptr = ptr->next)
      {
        Clay_String txt;
        if(ptr->string_len > 0){
          txt = (Clay_String) {
            .length = ptr->string_len,
            .chars = ptr->string
          };
        }else{
          char idx_str[10];
          txt = (Clay_String) {
            .length = snprintf(idx_str, 10, "%d", i),
            .chars = idx_str
          };
        }
        RenderKeyLabel(txt, ptr);
        i++;
      }
    }
    
    CLAY({
      .id = CLAY_ID("Values"),
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

      if (appstate->currentNode != appstate->json)
      {
        CLAY({
          .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = {
              .width = CLAY_SIZING_GROW(),
              .height = CLAY_SIZING_FIXED(1)
            }
          },
        }) {
          CLAY_TEXT(CLAY_STRING("Content for: "), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
          Clay_String keyname = (Clay_String) {
            .length = appstate->currentNode->string_len,
            .chars = appstate->currentNode->string
          };
          CLAY_TEXT(keyname, CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
        }
      }else{
        CLAY_TEXT(CLAY_STRING("Select an Item"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
      }

      if (appstate->currentNode == NULL) {
        CLAY_TEXT(
          CLAY_STRING("nothing to show"),
            CLAY_TEXT_CONFIG({
              .wrapMode = CLAY_TEXT_WRAP_WORDS,
              .textColor = COLOR_WHITE
            })
        );
      } else {
        cJSON_PrintPreallocated(appstate->currentNode, appstate->main_screen_buffer, 80*24, 1);

        Clay_String valueforkey = (Clay_String) {
          .length = strlen(appstate->main_screen_buffer),
          .chars = appstate->main_screen_buffer
        };

        CLAY_TEXT(
          valueforkey,
            CLAY_TEXT_CONFIG({
              .wrapMode = CLAY_TEXT_WRAP_WORDS,
              .textColor = COLOR_WHITE
            })
        );
      }
 
      // char result[128];
      // sprintf(result, "MX: %d MY: %d MC: %d F:%d fps: %.5f", mouseX, mouseY, clickActive, frameCount, renderSpeed > 0 ? 1/renderSpeed : 0);
      // Clay_String txt = (Clay_String) {
      //   .length = strlen(result),
      //   .chars = result
      // };
      
      // CLAY_TEXT(
      //   txt,
      //     CLAY_TEXT_CONFIG({
      //       .wrapMode = CLAY_TEXT_WRAP_WORDS,
      //       .textColor = COLOR_GREEN
      //     })
      // );
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
  free(appstate.json);
  free(appstate.main_screen_buffer);

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
  raw.c_cc[VTIME] = 1;
  raw.c_cc[VMIN] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  uncook();
}

void setTermSize(Clay_Dimensions* wsize) {
    wsize->width = 80;
    wsize->height = 24;
    return;

    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
      return;
    wsize->width = (CLAY__FLOAT) w.ws_col;
    wsize->height = (CLAY__FLOAT) w.ws_row - 1;
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
          clickActive = (ptr[2] == 0x20);
          // -1 is important, we use 0-based indexing, the shell uses 1-based indexing
          mouseX = ptr[3] - 32 - 1;
          mouseY = ptr[4] - 32 - 1;
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
    case ' ':
    case '\r':
      clickActive = !clickActive;
      break;
    case ARROW_UP:
      mouseY = CLAY__MAX(0, mouseY - 1);
      break;
    case ARROW_DOWN:
      mouseY = CLAY__MIN(wsize.height, mouseY + 1);
      break;
    case ARROW_LEFT:
      mouseX = CLAY__MAX(0, mouseX - 1);
      break;
    case ARROW_RIGHT:
      mouseX = CLAY__MIN(wsize.width, mouseX + 1);
      break;
    case (ARROW_LEFT+0x1f):
      mouseX = CLAY__MAX(0, mouseX - 10);
      break;
    case (ARROW_UP+0x1f):
      mouseY = CLAY__MAX(0, mouseY - 10);
      break;
    case (ARROW_DOWN+0x1f):
      mouseY = CLAY__MIN(wsize.height, mouseY + 5);
      break;
    case (ARROW_RIGHT+0x1f):
      mouseX = CLAY__MIN(wsize.width, mouseX + 10);
      break;
  }
}

void DrawFrame(ApplicationState* appstate) {
  Clay_RenderCommandArray layout = CreateLayout(appstate);
  Clay_Console_Render(layout, wsize.width, wsize.height);
  fflush(stdout);
}

int main(int argc, char** argv) {

  if (argc < 2)
  {
    printf("no json file provided\n");
    exit(1);
  }

  char* source = (char *)malloc(10000);
  FILE *fp = fopen(argv[1], "r");
  if(fp != NULL)
  {
      int idx = 0;
      char symbol;
      while(( symbol= getc(fp)) != EOF)
      {
        source[idx++] = symbol;
      }
      fclose(fp);
  }
  appstate.json = cJSON_Parse(source);
  appstate.main_screen_buffer = (char *)malloc(80*24);
  appstate.stack[appstate.curridx] = appstate.json;
  appstate.currentNode = appstate.json;

  if (appstate.json == NULL)
  {
    cook();
    printf("error parsing json on offset %d\n", global_error.position);
    printf("%.*s\n", (global_error.position+2), global_error.json);
    printf("%*s", global_error.position, "^");
    fflush(stdout);
    exit(1);
  }

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
    Clay_SetPointerState((Clay_Vector2) { mouseX, mouseY }, clickActive);
    DrawFrame(&appstate);
    editorProcessKeypress();
  }
}