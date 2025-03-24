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
// // #define CLAY_DISABLE_SIMD 1
// #define CLAY__EPSILON 0

#include "clay.h"

#include "helper.h"
#include "app.h"

#include "clay_renderer_terminal.c"

const Clay_Color COLOR_BLACK =  {0, 0, 0, 255};
const Clay_Color COLOR_RED =  {255, 0, 0, 255};
const Clay_Color COLOR_GREEN =  {0, 127, 0, 255};
const Clay_Color COLOR_BLUE =   {0, 0, 255, 255};
const Clay_Color COLOR_LIGHT =  {100, 100, 100, 255};

static uint64_t frameCount = 0;
static float renderSpeed = 0;
// TODO: move these to the appstate
int mouseMode = 1;
int clickActive = 0;

Clay_Dimensions wsize = {
  .width = (CLAY__FLOAT) 80,
  .height = (CLAY__FLOAT) 24,
};

Clay_ElementDeclaration CED_JsonItemConfig(cJSON* node, ApplicationState* appstate, bool hovered) {
  return CLAY__INIT(Clay_ElementDeclaration){
    .layout = {
    .padding = CLAY_PADDING_ALL(0),
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        .width = CLAY_SIZING_GROW(),
        .height = CLAY_SIZING_FIT(1)
      }
    },
    .backgroundColor = hovered ? (appstate->currentNode == node ? COLOR_BLUE : COLOR_BLUE) : COLOR_BLACK,
  };
}

Clay_ElementDeclaration CED_sidebar_config() {
  return CLAY__INIT(Clay_ElementDeclaration) {
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
  };
}

Clay_ElementDeclaration CED_main_view_config() {
  return CLAY__INIT(Clay_ElementDeclaration) {
    .id = CLAY_ID("Values"),
    .border = { .width = { 0, 1, 1, 1, 0}, .color = COLOR_WHITE },
    .layout = {
      .padding = { 0, 1, 1, 1},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        .width = CLAY_SIZING_GROW(),
        .height = CLAY_SIZING_GROW()
      }
    },
    .scroll = { .vertical = 1, .horizontal = 1 },
    .backgroundColor = COLOR_LIGHT
  };
}

Clay_ElementDeclaration CED_InspectViewConfig() {
  return CLAY__INIT(Clay_ElementDeclaration) {
    .layout = {
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
      },
    },
  };
}

void RenderSidebarJsonItemLabel(Clay_String txt, ApplicationState* appstate, cJSON* node){
  CLAY(CED_JsonItemConfig(node, appstate, Clay_Hovered())) {
    Clay_OnHover(APP_json_item_on_hover, (intptr_t)node);
    CLAY_TEXT(txt, CLAY_TEXT_CONFIG({ .wrapMode = CLAY_TEXT_WRAP_WORDS, .textColor = COLOR_WHITE }));
  }
}

// TODO: move this to the appstate
void RenderSidebarJsonPathLabel(ApplicationState* appstate, cJSON* topmost_node){
  CLAY({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), } } }) {
    if (!cjson_is_subscriptable(topmost_node)) {
      return;
    }

    CLAY_TEXT(
      clay_string_for_json_subscript(topmost_node, appstate->current_key_info_label),
      CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE })
    );
  }
}

void RenderSidebarJsonPathSeparator() {
  CLAY_TEXT(CLAY_STRING("---"), CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE}));
} 

void RenderSidebar(ApplicationState* appstate){
  Clay_String keyname = clay_string_for_json_path(appstate);
  cJSON* topmost_node = appstate->stack[appstate->curridx];

  RenderSidebarJsonItemLabel(keyname, appstate, topmost_node);
  RenderSidebarJsonPathLabel(appstate, topmost_node);
  RenderSidebarJsonPathSeparator();

  int i = 0;
  for (struct cJSON* ptr = topmost_node->child; ptr != NULL; ptr = ptr->next) {
    Clay_String txt = clay_string_for_json_key(ptr, i);
    RenderSidebarJsonItemLabel(txt, appstate, ptr);
    i++;
    if (i > 300)
    {
      Clay_String txt = CLAY_STRING("TODO: Render more keys with no OOM");
      CLAY_TEXT(txt, CLAY_TEXT_CONFIG({ .wrapMode = CLAY_TEXT_WRAP_WORDS, .textColor = COLOR_RED }));
      break;
    }
  }
}

void RenderMainContent(ApplicationState* appstate) {
  if (appstate->currentNode == NULL) {
    CLAY_TEXT(
      CLAY_STRING("nothing to show"),
      CLAY_TEXT_CONFIG({.wrapMode = CLAY_TEXT_WRAP_WORDS, .textColor = COLOR_WHITE})
    );
    return;
  }
  // Commenting here the scroll container experimentation
  // Clay_ElementId vlid = Clay_GetElementId(CLAY_STRING("Values"));
  // // Clay_ElementData vbox = Clay_GetElementData(vlid);
  // Clay_ScrollContainerData scd = Clay_GetScrollContainerData(vlid);

  // int first_newline = CLAY__MAX(0, scd.found ? (-scd.scrollPosition->y)-2 : 0);
  // int last_newline = scd.found ? scd.scrollContainerDimensions.height : 24;
  // char* start = find_nth_newline_offset(appstate->main_screen_buffer, first_newline);
  // char* end = find_nth_newline_offset(start, last_newline);
  // int length = end-start;

  // Clay_String newline = CLAY_STRING("\n");
  CLAY({
    .layout = {
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = {
        .width = CLAY_SIZING_GROW(),
        .height = CLAY_SIZING_GROW()
      }
    }
  }) {
    Clay_String str = (Clay_String) {
      // .length = appstate->main_screen_buffer_len,
      // until we learn how to render more text without running out of ram,
      // we'll only keep 10kb of text.
      .length = CLAY__MIN(appstate->main_screen_buffer_len, 10*1024),
      .chars = appstate->main_screen_buffer
    };

    CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = COLOR_WHITE, .hashStringContents = false }));
    if (appstate->main_screen_buffer_len > 10*1024){
      CLAY_TEXT(CLAY_STRING("TODO: Learn how to show more text without OOM"), CLAY_TEXT_CONFIG({ .textColor = COLOR_RED, .hashStringContents = false }));
    }
  }

}

void RenderJsonView(ApplicationState* appstate) {
  App_BuildJsonPathStr(appstate, 0);
  CLAY(CED_sidebar_config()) {
    RenderSidebar(appstate);
  }
  CLAY(CED_main_view_config()) {
    RenderMainContent(appstate);
  }
}

void RenderGrowHVBlock(){
  CLAY({ .layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, }});
}

void RenderInspectView(ApplicationState* appstate){
  App_BuildJsonPathStr(appstate, 1);
  CLAY(CED_InspectViewConfig()) {
    RenderGrowHVBlock();
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
        CLAY_STRING("Current JQ Path:"),
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
    RenderGrowHVBlock();
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
  EnableMouseMode();
  printf("\x1B[?1049h"); // save cursor and enable alternative buffer.
}

struct termios orig_termios;

void disableRawMode() {
  free(APP_global_state.json);
  free(APP_global_state.main_screen_buffer);

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

enum editorKey {
  ARROW_LEFT = 0x1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
};

char rawRead() {
  char c;
  int nread = read(STDIN_FILENO, &c, 1);
  if (nread == -1 && errno != EAGAIN) die("read");
  return c;
}

int editorReadKey() {
  char seq[10] = {0};
  int bytesread = read(STDIN_FILENO, seq, 10);
  char *ptr = seq;

  if (*ptr == '\x1b') {
    ptr += 1;
    if (*ptr == '[') {
      int ctrl = 0;

      reparse_char:
      switch (ptr[1]) {
        case '6':
          return PAGE_DOWN;
          break;
        case '5':
          return PAGE_UP;
          break;
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
          APP_global_state.mouseX = ptr[3] - 32 - 1;
          APP_global_state.mouseY = ptr[4] - 32 - 1;
        }
      }
    }
    if (bytesread == 1)
    {
      return '\x1b';
    }
    return '\0';
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
      APP_global_state.showHelp = !APP_global_state.showHelp;
      break;
    case CTRL_KEY('l'):
      (mouseMode) ? DisableMouseMode() : EnableMouseMode();
      break;
    case CTRL_KEY('i'):
      APP_global_state.inspectPath = !APP_global_state.inspectPath;
      if (APP_global_state.inspectPath){
        DisableMouseMode();
      }else{
        EnableMouseMode();
      }
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
    case PAGE_DOWN:
        Clay_UpdateScrollContainers(false, (Clay_Vector2) { 0, -2 }, 0.2);
      break;
    case '\x1b':
      App_PopAndInspectParentNode(&APP_global_state);
      break;
    case PAGE_UP:
        Clay_UpdateScrollContainers(false, (Clay_Vector2) { 0, 2 }, 0.2);
      break;
    case ARROW_UP:
      APP_global_state.mouseY = CLAY__MAX(0, APP_global_state.mouseY - 1);
      break;
    case ARROW_DOWN:
      APP_global_state.mouseY = CLAY__MIN(wsize.height - 1, APP_global_state.mouseY + 1);
      break;
    case ARROW_LEFT:
      APP_global_state.mouseX = CLAY__MAX(0, APP_global_state.mouseX - 1);
      break;
    case ARROW_RIGHT:
      APP_global_state.mouseX = CLAY__MIN(wsize.width - 1, APP_global_state.mouseX + 1);
      break;
    case (ARROW_LEFT+0x1f):
      APP_global_state.mouseX = CLAY__MAX(0, APP_global_state.mouseX - 10);
      break;
    case (ARROW_UP+0x1f):
      APP_global_state.mouseY = CLAY__MAX(0, APP_global_state.mouseY - 10);
      break;
    case (ARROW_DOWN+0x1f):
      APP_global_state.mouseY = CLAY__MIN(wsize.height - 1, APP_global_state.mouseY + 5);
      break;
    case (ARROW_RIGHT+0x1f):
      APP_global_state.mouseX = CLAY__MIN(wsize.width - 1, APP_global_state.mouseX + 10);
      break;
  }
}

void DrawFrame(ApplicationState* appstate) {
  // CreateLayout(appstate);
  Clay_RenderCommandArray layout = CreateLayout(appstate);
  Clay_Console_Render(layout, wsize.width, wsize.height);
  fflush(stdout);
}

void HandleClayErrors(Clay_ErrorData errorData) {
  // See the Clay_ErrorData struct for more information
  cook();
  fflush(stdout);
  printf("ERR-%d: %s", errorData.errorType, errorData.errorText.chars);
  fflush(stdout);
  exit(1);
}

void APP_JsonPrintAndMeasure(ApplicationState* appstate){
  cJSON_PrintPreallocated(appstate->currentNode, appstate->main_screen_buffer, 20000000, 1);
  size_t len = strlen(appstate->main_screen_buffer);
  appstate->main_screen_buffer_len = len;

  Clay_Dimensions computed = Console_MeasureText(CLAY__INIT(Clay_StringSlice) {
    .length = len,
    .chars = appstate->main_screen_buffer,
    .baseChars = appstate->main_screen_buffer
  }, 0, 0);
  appstate->jsonsize.width = computed.width;
  appstate->jsonsize.height = computed.height;
}

int main(int argc, char** argv) {
  if (argc < 2)
  {
    printf("no json file provided\n");
    exit(1);
  }

  APP_global_state.current_key_info_label = (char *)malloc(MAX_LABEL_KEYINFO_SIZE);

  // very long buffer to load really crazy files
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
  // This prolly 
  APP_global_state.json = cJSON_Parse(source);
  APP_global_state.main_screen_buffer = source;
  APP_global_state.json_path_buffer = (char *)malloc(80);
  APP_global_state.json_path_buffer_len = 0;
  APP_global_state.stack[APP_global_state.curridx] = APP_global_state.json;
  APP_global_state.currentNode = APP_global_state.json;
  APP_JsonPrintAndMeasure(&APP_global_state);

  if (APP_global_state.json == NULL)
  {
    cook();
    printf("error parsing json on offset %ld\n", global_error.position);
    printf("%.*s\n", (int)(global_error.position+2), global_error.json);
    printf("%*s", (int)global_error.position, "^");
    fflush(stdout);
    exit(1);
  }
  App_BuildJsonPathStr(&APP_global_state, 0);

  // neither this, yes it's a lot
  // uint64_t totalMemorySize = Clay_MinMemorySize()*1024;
  // didn't reall work
  Clay_SetMaxElementCount(Clay__defaultMaxElementCount);
  Clay_SetMaxMeasureTextCacheWordCount(Clay__defaultMaxMeasureTextWordCacheCount*2);

  uint64_t totalMemorySize = Clay_MinMemorySize();
  printf("Allocating %ld bytes\n", totalMemorySize);

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

  enableRawMode();
  setTermSize(&wsize);

  while (1){
    cJSON* old = APP_global_state.currentNode;
    Clay_SetPointerState((Clay_Vector2) { APP_global_state.mouseX, APP_global_state.mouseY }, clickActive);

    DrawFrame(&APP_global_state);

    Clay_ElementId values_id = Clay_GetElementId(CLAY_STRING("Values"));
    Clay_ScrollContainerData scd_values = Clay_GetScrollContainerData(values_id);
    
    if (old != APP_global_state.currentNode){
      if (scd_values.found){
        scd_values.scrollPosition->y = 0;
      }
      memset(source, 0, 20000000);
      APP_JsonPrintAndMeasure(&APP_global_state);
    }

    editorProcessKeypress();
  }
}