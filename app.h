#define MAX_DEPTH 10
#define MAX_LABEL_KEYINFO_SIZE 24

typedef struct {
  // These need to be freed
  cJSON* json;
  char* main_screen_buffer;
  size_t main_screen_buffer_len;
  bool inspectPath;
  bool showHelp;
  size_t json_path_buffer_len;
  char* json_path_buffer;
  char* current_key_info_label;
  size_t current_key_info_label_len;
  // These are weak references
  cJSON* currentNode;
  cJSON* parent;
  cJSON* stack[MAX_DEPTH];
  size_t curridx;
  Clay_Dimensions jsonsize;
  int mouseX;
  int mouseY;
} ApplicationState;

static ApplicationState APP_global_state = {
  .showHelp = 0,
  .inspectPath = 0,
  .main_screen_buffer = NULL,
  .json_path_buffer = NULL,
  .json_path_buffer_len = 0,
  .json = NULL,
  .currentNode = NULL,
  .parent = NULL,
  .stack = {0},
  .curridx = 0,
  .mouseX = 0,
  .mouseY = 0
};


typedef struct {
  // These need to be freed
  ApplicationState* appstate;
  cJSON* current_json_node;
} HoverHandlerDTO;


void App_PushAndInspectCurrentNode(ApplicationState* appstate, cJSON* node);
bool App_NodeIsParent(ApplicationState* appstate, cJSON* node);
void App_PopAndInspectParentNode(ApplicationState* appstate);
void App_BuildJsonPathStr(ApplicationState *appstate, int compact);

Clay_String clay_string_for_json_path(ApplicationState* appstate);
Clay_String clay_string_for_json_key(cJSON* node, int fallback);


/// implementations

const Clay_Color COLOR_WHITE =  {255, 255, 255, 255};

void App_PushAndInspectCurrentNode(ApplicationState* appstate, cJSON* node) {
  if ((appstate->curridx+1) >= MAX_DEPTH)
  {
    return; 
  }

  appstate->stack[++appstate->curridx] = appstate->currentNode;
  appstate->currentNode = node;
  App_BuildJsonPathStr(appstate, 0);

  appstate->mouseX = 1;
  appstate->mouseY = 3;
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
  Clay_ElementId keys_id = Clay_GetElementId(CLAY_STRING("Keys"));
  Clay_ScrollContainerData scd_keys = Clay_GetScrollContainerData(keys_id);
  if (scd_keys.found){
    scd_keys.scrollPosition->x = 0;
  }

  appstate->mouseX = 2;
  appstate->mouseY = 4;
}

void App_BuildJsonPathStr(ApplicationState *appstate, int compact){
  appstate->json_path_buffer_len = 0;
  if (!compact) {
    appstate->json_path_buffer_len = snprintf(appstate->json_path_buffer, 80, "%c", '$');
  }

  for (size_t i = 1; i <= appstate->curridx; i++){
    cJSON* node = appstate->stack[i];
    int hasSpace = 0;
    if(node->string_len > 0){
      hasSpace = strchr(node->string, ' ') != NULL;
    }

    appstate->json_path_buffer_len += snprintf(
      appstate->json_path_buffer + appstate->json_path_buffer_len,
      80 - appstate->json_path_buffer_len,
      compact ? hasSpace ? "[\"%s\"]" : (node->string_len > 0 ? ".%s" : "%s") : " > %s",

      node->string_len > 0 
        ? node->string
        : "[]"
    );
  }
}

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}


void APP_json_item_on_hover(Clay_ElementId UNUSED(elementId), Clay_PointerData pointerInfo, intptr_t userData) {
  cJSON * node = (cJSON *)userData;
  if (node == NULL){
    die("Lol");
  }
  APP_global_state.currentNode = node;

  // When clicked: 
  if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME){
    if (App_NodeIsParent(&APP_global_state, node)) {
      App_PopAndInspectParentNode(&APP_global_state);
      return;
    }

    if (cjson_is_subscriptable(node)){
      App_PushAndInspectCurrentNode(&APP_global_state, node);
    }
  }
}

Clay_String clay_string_for_json_key(cJSON* node, int fallback){
  Clay_String txt;

  if(node->string_len <= 0){
    txt = Clay__IntToString(fallback);
  }else{
    cjson_key_name_to_clay_string(node, &txt);
  }

  return txt;
}

Clay_String clay_string_for_json_subscript(cJSON* node, char* keycountlbl){
  int keycount = cjson_children_count(node->child);
  Clay_String str;

  if (cJSON_IsObject(node)){
    str.length = snprintf(keycountlbl, MAX_LABEL_KEYINFO_SIZE, "Object <%d>", keycount);
  }else if (cJSON_IsArray(node)){
    str.length = snprintf(keycountlbl, MAX_LABEL_KEYINFO_SIZE, "Array <%d>", keycount);
  }
  str.chars = keycountlbl;
  return str;
}


Clay_String clay_string_for_json_path(ApplicationState* appstate){
  return CLAY__INIT(Clay_String) {
    .length = appstate->json_path_buffer_len,
    .chars = appstate->json_path_buffer
  };
}
