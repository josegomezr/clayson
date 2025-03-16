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
#include <string.h>
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

const Clay_Color COLOR_BLACK =  {0, 0, 0, 255};
const Clay_Color COLOR_WHITE =  {255, 255, 255, 255};
const Clay_Color COLOR_RED =  {255, 0, 0, 255};
const Clay_Color COLOR_GREEN =  {0, 127, 0, 255};
const Clay_Color COLOR_BLUE =   {0, 0, 255, 255};
const Clay_Color COLOR_LIGHT =  {100, 100, 100, 255};

static uint64_t frameCount = 0;
static float renderSpeed = 0;
int mouseMode = 1;
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

// // An example function to begin the "root" of your layout tree
int button1 = 0;
int button2 = 0;
int button3 = 0;

#define MAX_DEPTH 10

typedef struct {
  // These need to be freed
  cJSON* json;
  char* main_screen_buffer;
  bool inspectPath;
  bool showHelp;
  size_t json_path_buffer_len;
  char* json_path_buffer;

  // These are weak references
  cJSON* currentNode;
  cJSON* parent;
  cJSON* stack[MAX_DEPTH];
  size_t curridx;
} ApplicationState;
void App_PushAndInspectCurrentNode(ApplicationState* appstate, cJSON* node);
bool App_NodeIsParent(ApplicationState* appstate, cJSON* node);
void App_PopAndInspectParentNode(ApplicationState* appstate);
void App_BuildJsonPathStr(ApplicationState *appstate, int compact);

static ApplicationState appstate = {
  .showHelp = 0,
  .inspectPath = 0,
  .main_screen_buffer = NULL,
  .json_path_buffer = NULL,
  .json_path_buffer_len = 0,
  .json = NULL,
  .currentNode = NULL,
  .parent = NULL,
  .stack = {0},
  .curridx = 0
};

void App_PushAndInspectCurrentNode(ApplicationState* appstate, cJSON* node) {
  if ((appstate->curridx+1) >= MAX_DEPTH)
  {
    return; 
  }

  appstate->stack[++appstate->curridx] = appstate->currentNode;
  appstate->currentNode = node;
  App_BuildJsonPathStr(appstate, 0);
}

bool App_NodeIsParent(ApplicationState* appstate, cJSON* node){
  return appstate->stack[appstate->curridx] == node;
}

void App_PopAndInspectParentNode(ApplicationState* appstate){
  if(appstate->curridx <= 0) {
    return;
  }

  appstate->stack[appstate->curridx] = NULL;
  appstate->curridx = CLAY__MAX(0, appstate->curridx-1);
  App_BuildJsonPathStr(appstate, 0);
}

void App_BuildJsonPathStr(ApplicationState *appstate, int compact){
  appstate->json_path_buffer_len = snprintf(appstate->json_path_buffer, 80, "%c", compact ? '.' : '$');
  for (size_t i = 1; i <= appstate->curridx; i++){
    cJSON* node = appstate->stack[i];
    int hasSpace = 0;
    if(node->string_len > 0){
      hasSpace = strchr(node->string, ' ') != NULL;
    }

    appstate->json_path_buffer_len += snprintf(
      appstate->json_path_buffer + appstate->json_path_buffer_len,
      80 - appstate->json_path_buffer_len,
      compact ? hasSpace ? "[\"%s\"]" : ".%s" : " > %s",

      node->string_len > 0 
        ? node->string
        : "[]"
    );
  }
}

void HandleButtonInteraction(Clay_ElementId UNUSED(elementId), Clay_PointerData pointerInfo, intptr_t userData) {
  cJSON * node = (cJSON *)userData;

  // On hover, show the currently selected key
  appstate.currentNode = node;

  // When clicked: 
  if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME){
    if (App_NodeIsParent(&appstate, node)) {
      App_PopAndInspectParentNode(&appstate);
      return;
    }

    if (cJSON_IsObject(node) || cJSON_IsArray(node)){
      App_PushAndInspectCurrentNode(&appstate, node);
    }
  }

  if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME){
  }
}

void json_key_name(cJSON* ptr, Clay_String* ref){
  ref->length = ptr->string_len;
  ref->chars = ptr->string;
}


void RenderKeyLabel(Clay_String txt, cJSON* node){
  CLAY({
      .layout = {
      .padding = CLAY_PADDING_ALL(0),
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = {
          .width = CLAY_SIZING_GROW(),
          .height = CLAY_SIZING_FIT(1)
        }
      },
      // .backgroundColor = Clay_Hovered() ? (appstate.currentNode == node ? COLOR_BLUE : COLOR_BLUE) : COLOR_BLACK,
    }) {
      Clay_OnHover(HandleButtonInteraction, (intptr_t)node);
      CLAY_TEXT(txt, CLAY_TEXT_CONFIG({ .wrapMode = CLAY_TEXT_WRAP_WORDS, .textColor = COLOR_WHITE }));
    } 
}

void RenderSidebar(ApplicationState* appstate){
  // CLAY_TEXT(CLAY_STRING("Keys"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
    Clay_String keyname = (Clay_String) {
      .length = appstate->json_path_buffer_len,
      .chars = appstate->json_path_buffer
    };

    RenderKeyLabel(keyname, appstate->stack[appstate->curridx]);
    CLAY_TEXT(CLAY_STRING("---"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));

    int i = 0;
    for (struct cJSON* ptr = (appstate->stack[appstate->curridx])->child; ptr != NULL; ptr = ptr->next)
    {
      Clay_String txt;
      if(ptr->string_len <= 0){
        txt = Clay__IntToString(i);
      }else{
        json_key_name(ptr, &txt);
      }
      
      RenderKeyLabel(txt, ptr);
      i++;
    }
}

void RenderMainContent(ApplicationState* appstate) {
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
}

void RenderJsonView(ApplicationState* appstate) {
  App_BuildJsonPathStr(appstate, 0);
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
      .scroll = { .vertical = 1 },
      // .backgroundColor = COLOR_RED,
      .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = COLOR_WHITE }
  }) {
    RenderSidebar(appstate);
  }
  
  CLAY({
    .id = CLAY_ID("Values"),
    .border = { .width = { 0, 1, 1, 1, 0}, .color = COLOR_WHITE },
    .layout = {
      .padding = { 0, 1, 1, 1},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        .width = CLAY_SIZING_GROW(),
        .height = CLAY_SIZING_PERCENT(1)
      }
    },
    .scroll = { .vertical = 1 },
    .backgroundColor = COLOR_LIGHT
  }) {
    Clay_String txt = (Clay_String) {
      .length = snprintf(appstate->json_path_buffer+(appstate->json_path_buffer_len >= 80 ? 80 : appstate->json_path_buffer_len), 80-appstate->json_path_buffer_len, "MX: %d MY: %d MC: %d F:%ld fps: %.5f", mouseX, mouseY, clickActive, frameCount, renderSpeed > 0 ? 1/renderSpeed : 0),
      .chars = appstate->json_path_buffer+(appstate->json_path_buffer_len >= 80 ? 80 : appstate->json_path_buffer_len)
    };
    
    CLAY_TEXT(
      txt,
        CLAY_TEXT_CONFIG({
          .wrapMode = CLAY_TEXT_WRAP_WORDS,
          .textColor = COLOR_GREEN
        })
    );

    RenderMainContent(appstate);
  }
}

void RenderInspectView(ApplicationState* appstate){
  App_BuildJsonPathStr(appstate, 1);
  CLAY({
    .layout = {
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
      },
    },
  }) {
    CLAY({ .layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, }});
    CLAY({
      .layout = {
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = {
          .height = CLAY_SIZING_FIT(),
          .width = CLAY_SIZING_GROW(),
        },
        .padding = CLAY_PADDING_ALL(1),
      },
      // .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = COLOR_WHITE }
    }) {
      CLAY_TEXT(
        CLAY_STRING("Current JSON Path:"),
        CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_RIGHT,})
      );

      CLAY({
        .layout = {
          .layoutDirection = CLAY_LEFT_TO_RIGHT,
          .sizing = {
            CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
          },
        },
      }){
        Clay_String keyname = (Clay_String) {
          .length = appstate->json_path_buffer_len,
          .chars = appstate->json_path_buffer
        };

        CLAY_TEXT(keyname, CLAY_TEXT_CONFIG({
          .textColor = COLOR_WHITE,
          .textAlignment = CLAY_TEXT_ALIGN_CENTER,
          })
        );
      }
    
     }
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, }});
  }
}

void RenderHelpView(){
  CLAY({
    .layout = {
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
      },
    },
  }) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, }});

    CLAY_TEXT(
      CLAY_STRING("Control + I => Inspect current json path in jq-like format"),
      CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_RIGHT,})
    );

    CLAY_TEXT(
      CLAY_STRING("Control + H => show this help"),
      CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_RIGHT,})
    );


    CLAY_TEXT(
      CLAY_STRING("Control + Q => exit"),
      CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .textAlignment = CLAY_TEXT_ALIGN_RIGHT,})
    );

    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, }});
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
    if (appstate->inspectPath){
      RenderInspectView(appstate);
    }else if (appstate->showHelp){
      RenderHelpView(appstate);
    }else{
      RenderJsonView(appstate);
    }
  }
  
  renderSpeed = ((CLAY__FLOAT)clock()/CLOCKS_PER_SEC) - startTime;

  return Clay_EndLayout();
}


void HandleClayErrors(Clay_ErrorData errorData) {
    // See the Clay_ErrorData struct for more information
    printf("ERR-%d: %s", errorData.errorType, errorData.errorText.chars);
}

void EnableMouseMode(){
  mouseMode = 1;
  printf("\x1B[?1000h"); // MouseMode
}

void DisableMouseMode(){
  mouseMode = 0;
  printf("\x1B[?1000l"); // MouseMode
}

void cook() {
  printf("\x1B[?1049l"); // Reenable cursor and disable alternative buffer
  printf("\x1B[?47l"); // Restore screen.
  printf("\x1B[u"); // Restore cursor position.
  DisableMouseMode();
  printf("\x1B[?25h"); // Hide the cursor.
}

void uncook() {
  printf("\x1B[?25l"); // show the cursor.
  printf("\x1B[s"); // Save cursor position.
  printf("\x1B[?47h"); // Save screen.
  // EnableMouseMode();
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
  raw.c_cc[VTIME] = 0;
  raw.c_cc[VMIN] = 1;

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
    case CTRL_KEY('h'):
      appstate.showHelp = !appstate.showHelp;
      break;
    case CTRL_KEY('l'):
      (mouseMode) ? DisableMouseMode() : EnableMouseMode();
      break;
    case CTRL_KEY('i'):
      appstate.inspectPath = !appstate.inspectPath;
      break;
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
      mouseY = CLAY__MIN(wsize.height - 1, mouseY + 1);
      break;
    case ARROW_LEFT:
      mouseX = CLAY__MAX(0, mouseX - 1);
      break;
    case ARROW_RIGHT:
      mouseX = CLAY__MIN(wsize.width - 1, mouseX + 1);
      break;
    case (ARROW_LEFT+0x1f):
      mouseX = CLAY__MAX(0, mouseX - 10);
      break;
    case (ARROW_UP+0x1f):
      mouseY = CLAY__MAX(0, mouseY - 10);
      break;
    case (ARROW_DOWN+0x1f):
      mouseY = CLAY__MIN(wsize.height - 1, mouseY + 5);
      break;
    case (ARROW_RIGHT+0x1f):
      mouseX = CLAY__MIN(wsize.width - 1, mouseX + 10);
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

  char* source = (char *)malloc(20000000);
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
  appstate.json_path_buffer = (char *)malloc(80);
  appstate.json_path_buffer_len = 0;
  appstate.stack[appstate.curridx] = appstate.json;
  appstate.currentNode = appstate.json;

  if (appstate.json == NULL)
  {
    cook();
    printf("error parsing json on offset %ld\n", global_error.position);
    printf("%.*s\n", (int)(global_error.position+2), global_error.json);
    printf("%*s", (int)global_error.position, "^");
    fflush(stdout);
    exit(1);
  }
  App_BuildJsonPathStr(&appstate, 0);
  // appstate.json_path_buffer_len = sprintf(appstate.json_path_buffer, "a really long string a really long string a really");
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