#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Amiga compliant version string */
const char __ver[] = "\0$VER: StrTok 1.0 (30.12.2020) Brielle Harrison\0";

/* Some helpers to read a file into a C string */
#define FILE_OK 0
#define FILE_NOT_EXIST 1
#define FILE_TO_LARGE 2
#define FILE_READ_ERROR 3

//#pragma warning(disable : 4996)

//#define NON_AMIGA
#ifdef NON_AMIGA
  #ifndef max
  #define   max(a,b)    ((a) > (b) ? (a) : (b))
  #endif

  #ifndef min
  #define   min(a,b)    ((a) <= (b) ? (a) : (b))
  #endif

  /* The following bits do not exist outside the Amiga infrastructure
  and as such are simulated here. Linked lists are used frequently
  by the Amiga operating systems. We use one here to simulate the
  nearly limitless storage of string tokens that can be parsed; with
  the only real limit being memory. Standard library memory allocation
  is used to make the code more portable. */

  /* Some definitions that appear in either OS 4.x and in exec/types */
  typedef char* STRPTR;
  typedef unsigned long ULONG;
  typedef unsigned int uint8;
  typedef int int8;


  struct Node
  {
    struct Node* ln_Succ;
    struct Node* ln_Pred;
    uint8        ln_Type;
    int8         ln_Pri;
    STRPTR       ln_Name;
  };

  struct List
  {
    struct Node* lh_Head;
    struct Node* lh_Tail;
    struct Node* lh_TailPred;
    uint8        lh_Type;
    uint8        lh_Pad;
  };

  void AddTail(struct List* list, struct Node* node) {
    struct Node* last;

    if (list->lh_Head == NULL) {
      list->lh_Head = node;
      list->lh_TailPred = node;
    }

    last = list->lh_TailPred;
    if (last != node) {
      last->ln_Succ = node;
      node->ln_Pred = last;
      list->lh_TailPred = node;
    }
  }

  void NewList(struct List* list) { }
#else
  #include <exec/types.h>
  #include <proto/exec.h>
  #include <clib/alib_protos.h>
#endif

/* Structures */
typedef struct StringList {
  struct List strings;
  size_t count;

  void (*add)(struct StringList *self, const STRPTR string);
  STRPTR (*at)(struct StringList* self, size_t index);
} StringList;

typedef struct Range {
  size_t min;
  size_t max;
  size_t spread;
} Range;

typedef struct Arg {
  unsigned char value;
  unsigned char index;
} Arg;

typedef struct Args {
  Arg keep;
  Arg file;
  Arg help;
  Arg debug;
  STRPTR string;
  STRPTR token;
  Range range;
} Args;

/** Function declarations */
void PrintArgs(Args *args);
void ProcessArgs(Args *args, int argc, char **argv);
void FreeArgs(Args *args, unsigned char freeArgStructToo);
void ShowUsage(STRPTR command);
int wal_stricmp(const char* a, const char* b);
char* c_read_file(const char* f_name, int* err, size_t* f_size);
STRPTR stringdup(const STRPTR source);
StringList* NewStringList(void);
void FreeStringList(StringList* list);

int main(int argc, char** argv) {
  /* ReadArgs is not used to allow for 1.x compatibi lty */
  Args args;
  STRPTR part = NULL;
  StringList* parts = NewStringList();
  StringList* output = NewStringList();
  int rc = 0;

  /* Zero out stack variables */
  memset(&args, 0L, sizeof(Args));

  /* Process arguments */
  ProcessArgs(&args, argc, argv);
  if (args.help.value) {
    ShowUsage(argv[0]);
    return rc;
  }

  /* Show Argument Calcuation if Debug is set */
  if (args.debug.value) {
    PrintArgs(&args);
  }

  /* Split the string based on the supplied token */
  part = strtok(args.string, args.token);
  while (part != NULL) {
    parts->add(parts, part);

    if (args.keep.value && parts->strings.lh_TailPred) {
      struct Node* prev = parts->strings.lh_TailPred;
      STRPTR newBlock = NULL;

      newBlock = realloc(
         prev->ln_Name,
         strlen(prev->ln_Name) + strlen(args.token) + 1
      );

      if (newBlock) {
        prev->ln_Name = newBlock;
        prev->ln_Name = strcat(prev->ln_Name, args.token);
        prev->ln_Name = strcat(prev->ln_Name, "\0");
      }
    }

    part = strtok(NULL, args.token);
    parts->count++;
  }

  /** Show output */
  if (args.range.min >= 0 && args.range.max < parts->count) {
    size_t i;
    
    for (i = args.range.min; i <= args.range.max; i++) {
      printf(
        "%s%s",
        parts->at(parts, i),
        (
          args.keep.value ||
          args.range.spread == 1 ||
          i == args.range.max
        ) ? (char *)"" : (char *)args.token
      );
    }
  }
  else {
    if (args.debug.value) {
      printf(
        "Range %ld to %ld is out of bounds (%ld to %ld)\n",
        args.range.min,
        args.range.max,
        0L,
        parts->count
      );
    }
    rc = 5;
  }

  FreeStringList(parts);
  FreeStringList(output);
  FreeArgs(&args, 0);

  return 0;
}


char* c_read_file(const char* f_name, int* err, size_t* f_size)
{
  char* buffer;
  size_t length;
  FILE* f = fopen(f_name, "rb");
  size_t read_length;

  if (f)
  {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 1 GiB; best not to load a whole large file in one string
    if (length > 1073741824)
    {
      *err = FILE_TO_LARGE;

      return NULL;
    }

    buffer = (char*)malloc(length + 1);

    if (length)
    {
      read_length = fread(buffer, 1, length, f);

      if (length != read_length)
      {
        free(buffer);
        *err = FILE_READ_ERROR;

        return NULL;
      }
    }

    fclose(f);

    *err = FILE_OK;
    buffer[length] = '\0';
    *f_size = length;
  }
  else
  {
    *err = FILE_NOT_EXIST;

    return NULL;
  }

  return buffer;
}

int wal_stricmp(const char* a, const char* b) {
  int ca, cb;
  do {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    ca = tolower(toupper(ca));
    cb = tolower(toupper(cb));
  } while (ca == cb && ca != '\0');
  return ca - cb;
}

STRPTR stringdup(const STRPTR source) {
  STRPTR result = NULL;

  result = calloc(1, strlen(source) + 1);
  strcpy(result, source);

  return result;
}

void SL_add(struct StringList* self, const STRPTR string) {
  struct Node *node = NULL;

  node = calloc(1, sizeof(struct Node));

  node->ln_Name = calloc(1, strlen(string) + 1);
  if (!node->ln_Name) {
    free(node);
    return;
  }
  else {
    strcpy(node->ln_Name, string);
  }

  AddTail(&self->strings, node);
}

STRPTR SL_at(struct StringList* self, size_t index) {
  struct Node* target = self->strings.lh_Head;
  size_t i;

  for (i = 0; i < index && target != NULL; target = target->ln_Succ, i++);

  return target->ln_Name;
}

StringList* NewStringList() {
  StringList* result = calloc(1, sizeof(StringList));
  
  NewList((struct List *)result);
  result->add = SL_add;
  result->at = SL_at;

  return result;
}

void FreeStringList(StringList* list) {
  struct Node *node;
  
  for (node = list->strings.lh_Head; node != NULL;) {
    struct Node* nextNode = node->ln_Succ;
    free(node->ln_Name);
    free(node);
    node = nextNode;
  }

  free(list);
}

void ProcessArgs(Args *args, int argc, char **argv) {
  char **nu_argv = calloc(argc, sizeof(char *));
  char buffer[50];
  int nu_argc = argc;
  int index = 1;
  size_t i;
  Arg newline;
  Arg tab;
  Arg space;
  
  memset(&newline, 0L, sizeof(Arg));
  memset(&tab, 0L, sizeof(Arg));
  memset(&space, 0L, sizeof(Arg));

  /* Always include the name of the executable */
  nu_argv[0] = argv[0];

  /* If we don't even have a string, bail and print usage */
  if (argc < 2) {
    args->help.value = 1;
    return;
  }

  /* Process flags */
  for (i = 1; i < argc; i++) {
    /* The file flag toggles on the ability to pass in a file name instead
       of a raw string to process for splitting. Instead the contents of the
       supplied file are considered. */
    if (wal_stricmp(argv[i], "space") == 0) {
      space.value = 1;
      space.index = i;
      nu_argc--;
      continue;
    }

    if (wal_stricmp(argv[i], "tab") == 0) {
      tab.value = 1;
      tab.index = i;
      nu_argc--;
      continue;
    }

    if (wal_stricmp(argv[i], "newline") == 0) {
      newline.value = 1;
      newline.index = i;
      nu_argc--;
      continue;
    }
       
    if (wal_stricmp(argv[i], "file") == 0) {
      args->file.value = 1;
      args->file.index = i;
      nu_argc--;
      continue;
    }

    if (wal_stricmp(argv[i], "debug") == 0) {
      args->debug.value = 1;
      args->debug.index = i;
      nu_argc--;
      continue;
    }

    /* The keep flag indicates that the tokens themselves are to be included
       in the indexed string parts. The splitting token is appended to the
       previous string part. */
    if (wal_stricmp(argv[i], "keep") == 0) {
      args->keep.value = 1;
      args->keep.index = i;
      nu_argc--;
      continue;
    }

    if (
      wal_stricmp(argv[i], "?") == 0 ||
      wal_stricmp(argv[i], "help") == 0 ||
      wal_stricmp(argv[i], "-help") == 0 ||
      wal_stricmp(argv[i], "-h") == 0 ||
      wal_stricmp(argv[i], "--help") == 0 ||
      wal_stricmp(argv[i], "--h") == 0
    ) {
      args->help.value = 1;
      args->help.index = i;
      nu_argc--;
      continue;
    }

    /* If no flag was yet processed, consider it to be an argument and move on */
    nu_argv[index] = argv[i];
    index++;
  }

  if (nu_argc < 2) {
    return;
  }

  /* Process Token to split the string with/by */
  if (tab.value) {
    if (args->token) free(args->token);
    args->token = stringdup("\t");
  }
  else if (space.value) {
    if (args->token) free(args->token);
    args->token = stringdup(" ");
  }
  else if (newline.value) {
    if (args->token) free(args->token);
    args->token = stringdup("\n");
  }
  else if (
    (!tab.value && !space.value && !newline.value) &&
    (nu_argc < 4 && args->file.value)
  ) {
    if (args->token) free(args->token);
    args->token = stringdup("\n");
  }
  else {
    args->token = stringdup(nu_argc < 4 ? (char *)" " : (char *)nu_argv[3]);
  }

  /* Process Strings */
  if (args->file.value) {
    int err;
    size_t f_size;

    args->string = c_read_file(nu_argv[1], &err, &f_size);
  }
  else {
    args->string = stringdup(nu_argv[1]);
  }

  /* Prepare buffer with index converted to long converted back to string */
  sprintf(buffer, "%ld", atol(nu_argv[2]));

  /* Process Range/Desired Indices */
  if (nu_argc > 2 && strstr(nu_argv[2], ":") != NULL) {
    STRPTR index = nu_argv[2];
    STRPTR lower = calloc(1, strlen(index));
    STRPTR upper = calloc(1, strlen(index));
    STRPTR divide = strstr(index, ":");
    ULONG low = atol(lower);
    ULONG high = atol(upper);

    strncpy(lower, index, divide - index);
    strncpy(upper, divide + 1, strlen(divide + 1));

    args->range.min = min(low, high);
    args->range.max = max(low, high);
    args->range.spread = max(1, (args->range.max - args->range.min + 1));
  }
  else if (wal_stricmp(buffer, nu_argv[2]) == 0) {
    args->range.min = atol(nu_argv[2]);
    args->range.max = args->range.min;
    args->range.spread = 1;
  }
  else {
    args->range.min = 0;
    args->range.max = args->range.min;
    args->range.spread = 1;
  }
}

void PrintArgs(Args *args) {
  printf("Args\n");
  printf("  String length.....%ld\n", strlen(args->string));
  printf("  Token.............'%s'\n", args->token[0] == '\n' ? (char *)"\\n" : (char *)args->token);
  printf("  Range.............%ld to %ld\n", args->range.min, args->range.max);
  printf("Flags\n");
  printf("  Keep tokens.......%s\n", args->keep.value ? "true" : "false");
  printf("  File mode.........%s\n", args->file.value ? "true" : "false");
  printf("  Help mode.........%s\n", args->help.value ? "true" : "false");
}

void FreeArgs(Args *args, unsigned char freeArgStructToo) {
  free(args->string);
  free(args->token);

  if (freeArgStructToo) {
    free(args);
  }
}

void ShowUsage(STRPTR command) {
  printf("\nUsage: %s <string> <desired-index> <token> [KEEP] [HELP] [FILE]\n", command);
  printf("  <string>         the input string to work on\n");
  printf("  <desired-index>  a 0 based index indicating which part to keep\n");
  printf("  <token>          the string to split the input on (default: \" \")\n\n");
  printf("Flags: \n");
  printf("  [HELP, -h, ?]    HELP or -h or ? or -help will invoke this output\n");
  printf("  [KEEP]           when supplied, the tokens are appended to the preceeding\n");
  printf("                   index. So \"Hello,World\" splitting on \",\" would show\n");
  printf("                   index 0 as \"Hello,\" and index 1 as \"World\"\n\n");
  printf("  [FILE]           when the FILE mode is enabled, the <string> parameter is\n");
  printf("                   considered to be a path to a file to read. The default\n");
  printf("                   token used to split values is converted to a newline\n");
  printf("                   character (\"\\n\")\n\n");
  printf("Copyright (c)2020 Brielle Harrison. All Rights Reserved.\n\n");
}
