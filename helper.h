#ifdef __GNUC__
#define UNUSED(x) x __attribute__((unused))
#else
#define UNUSED(x) x
#endif

int cjson_children_count(cJSON* item);
int cjson_is_subscriptable(const cJSON * const node);
void cjson_key_name_to_clay_string(cJSON* ptr, Clay_String* ref);
char* find_nth_newline_offset(char* str, int count);

int cjson_children_count(cJSON* item){
  cJSON* chldptr = item;
  int keycount = 0;
  while(chldptr != NULL){
    keycount++;
    chldptr = chldptr->next;
  }
  return keycount;
}

int cjson_is_subscriptable(const cJSON * const node){
  return cJSON_IsObject(node) || cJSON_IsArray(node);
}

void cjson_key_name_to_clay_string(cJSON* ptr, Clay_String* ref){
  ref->length = ptr->string_len;
  ref->chars = ptr->string;
}

char* find_nth_newline_offset(char* str, int count){
  char* ptr = str;

  for (int i = 0; i < count; ++i)
  {
    char* next_newline = strchr(ptr, '\n');
    if (next_newline == NULL)
    {
      break;
    }

    ptr = next_newline + 1;
  }
  return ptr;
}
