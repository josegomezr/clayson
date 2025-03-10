// Must be defined in one file, _before_ #include "clay.h"
#define CLAY_IMPLEMENTATION
#define bool _Bool;

#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include "clay.h"
#include "clay_renderer_terminal.c"

const Clay_Color COLOR_LIGHT = (Clay_Color) {120, 120, 120, 120};
const Clay_Color COLOR_WHITE = (Clay_Color) {255, 255, 255, 255};
static uint64_t idx = 0;
// // An example function to begin the "root" of your layout tree
Clay_RenderCommandArray CreateLayout() {
	idx++;
	Clay_BeginLayout();

	CLAY({
		.id = CLAY_ID("OuterContainer"),
		.layout = {
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
			.sizing = {
				CLAY_SIZING_GROW(), CLAY_SIZING_GROW()
			},
		},
		.backgroundColor = {0, 0, 0, 255}
	}) {
		CLAY({
            .id = CLAY_ID("SideBar"),
            .layout = {
				.padding = CLAY_PADDING_ALL(1),
            	.layoutDirection = CLAY_TOP_TO_BOTTOM,
            	.sizing = {
            		.width = CLAY_SIZING_PERCENT(0.5),
            		.height = CLAY_SIZING_PERCENT(1)
            	}
            },
            .backgroundColor = COLOR_LIGHT,
            .border = { .width = { 1,2,1,1,1 }, .color = COLOR_WHITE }
        }) {

		}

		CLAY({
			.id = CLAY_ID("OtherSideBar"),
		    .layout = {
				.padding = CLAY_PADDING_ALL(2),
		    	.layoutDirection = CLAY_TOP_TO_BOTTOM,
		    	.sizing = {
		    		.width = CLAY_SIZING_PERCENT(0.5),
		    		.height = CLAY_SIZING_PERCENT(1)
		    	}
		    },
		    .backgroundColor = {0, 0, 0, 255}
		}) {
			char *result = (char*) malloc(128);
			sprintf(result, "0123456789 0123456 78901 234567 89012 34567 8901234567890 123456789::%d", idx);
			Clay_String txt = (Clay_String) {
				.length = strlen(result),
				.chars = result
			};
			
			// TODO font size is wrong, only one is allowed, but I don't know which it is
			CLAY_TEXT(
				txt,
			    CLAY_TEXT_CONFIG({ .fontId = 0, .fontSize = 24, .textColor = {255,255,255,255} })
			);
		}
	}

	return Clay_EndLayout();
}


void HandleClayErrors(Clay_ErrorData errorData) {
    // See the Clay_ErrorData struct for more information
    printf("ERR-%d: %s", errorData.errorType, errorData.errorText.chars);
}

void cook() {
	printf("\x1B[?25l"); // Hide the cursor.
	printf("\x1B[?1049l"); // Disable alternative buffer.
	printf("\x1B[?47l"); // Restore screen.
	printf("\x1B[u"); // Restore cursor position.
}

void uncook() {
	printf("\x1B[?25h"); // Hide the cursor.
	printf("\x1B[s"); // Save cursor position.
	printf("\x1B[?47h"); // Save screen.
	printf("\x1B[?1049h"); // Enable alternative buffer.
}

struct termios orig_termios;

void disableRawMode() {
  uncook();
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  cook();
}

int main() {
	const int width = 80;
	const int height = 24;

	enableRawMode();

	uint64_t totalMemorySize = Clay_MinMemorySize();
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
	Clay_Initialize(
		arena,
		(Clay_Dimensions) {
			.width = (float) width,
			.height = (float) height
		},
		(Clay_ErrorHandler) {
			HandleClayErrors
		}
	);
	// TODO this is wrong, but I have no idea what the actual size of the terminal is in pixels
	// // Tell clay how to measure text
	Clay_SetMeasureTextFunction(Console_MeasureText, NULL);
  	
  	char c;
	do {
    	Clay_RenderCommandArray layout = CreateLayout();

		Clay_Console_Render(layout, width, height);
		fflush(stdout);
  	} while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
}