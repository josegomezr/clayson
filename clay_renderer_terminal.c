
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#ifndef CLAY_HEADER
#include "clay.h"
#endif
#ifdef CLAY_OVERFLOW_TRAP
#include "signal.h"
#endif

static inline void Console_MoveCursor(int x, int y) {
	printf("\x1b[%d;%dH", y+1, x+1);
}
static inline void Console_Reset() {
	printf("\x1b[0m");
}
static inline void Console_Clear() {
	printf("\x1b[H\x1b[J"); // Clear
}

static inline void Console_SetFgColor(Clay_Color color) {
	printf("\x1b[38;2;%d;%d;%dm", (int)color.r, (int)color.g, (int)color.b);
}

static inline void Console_SetBgColor(Clay_Color color) {
	printf("\x1b[48;2;%d;%d;%dm", (int)color.r, (int)color.g, (int)color.b);
}

static inline bool Clay_PointIsInsideRect(Clay_Vector2 point, Clay_BoundingBox rect) {
	return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

static inline void Console_DrawBorder(int x0, int y0, int width, int height, Clay_Color color, Clay_BoundingBox scissorBox, char character) {
	Console_SetFgColor(color);
	for (int y = y0; y < (y0+height); y++) {
		for (int x = x0; x < (x0+width); x++) {
			if(!Clay__PointIsInsideRect((Clay_Vector2) { .x = x, .y = y }, scissorBox)) {
				continue;
			}

			Console_MoveCursor(x, y);
			putchar(character);
		}
	}
	Console_Reset();
}

static inline void Console_DrawRectangle(int x0, int y0, int width, int height, Clay_Color color, Clay_BoundingBox scissorBox, char character) {
	Console_SetBgColor(color);
	for (int y = y0; y < y0+height; y++) {
		for (int x = x0; x < x0+width; x++) {
			if(!Clay__PointIsInsideRect((Clay_Vector2) { .x = x, .y = y }, scissorBox)) {
				continue;
			}

			Console_MoveCursor(x, y);
			putchar(' ');
			// // TODO there are only two colors actually drawn, the background and white
			// if (color.a < 64) {
			// 	printf(".");
			// } else if (color.a < 128) {
			// 	printf("░");
			// } else if (color.a < 192) {
			// 	printf("▒");
			// } else {
			// 	printf("█");
			// }
		}
	}
	Console_Reset();
}

static inline Clay_Dimensions Console_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
	Clay_Dimensions textSize = { 0 };
	// TODO this function is very wrong, it measures in characters, I have no idea what is the size in pixels
	float maxTextWidth = 0.0f;
	float lineTextWidth = 0;

	float textHeight = 1;

	for (int i = 0; i < text.length; ++i)
	{
		if (text.chars[i] == '\n') {
			maxTextWidth = CLAY__MAX(maxTextWidth, lineTextWidth);
			lineTextWidth = 0;
			textHeight++;
			continue;
		}
		lineTextWidth++;
	}

	maxTextWidth = CLAY__MAX(maxTextWidth, lineTextWidth);

	textSize.width = maxTextWidth;
	textSize.height = textHeight;

	return textSize;
}

void Clay_Console_Render(Clay_RenderCommandArray renderCommands, Clay_Context* ctx)
{
	Console_Clear();

	int width = 80, height = 24;

	if (ctx != (Clay_Context*)0) {
		width = (ctx->layoutDimensions).width;
		height = (ctx->layoutDimensions).height;
	}

	const Clay_BoundingBox fullWindow = {
		.x = 0,
		.y = 0,
		.width = width,
		.height = height,
	};

	Clay_BoundingBox scissorBox = fullWindow;

	for (int j = 0; j < renderCommands.length; j++)
	{
		Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
		Clay_BoundingBox boundingBox = renderCommand->boundingBox;
		switch (renderCommand->commandType)
		{
			case CLAY_RENDER_COMMAND_TYPE_TEXT: {
				Clay_StringSlice text = renderCommand->renderData.text.stringContents;
				Clay_Color color = renderCommand->renderData.text.textColor;
				int y = 0;
				Console_SetFgColor(color);
				for (int x = 0; x < text.length; x++) {
					if(text.chars[x] == '\n') {
						y++;
						continue;
					}

					int cursorX = (int) boundingBox.x + x;
					int cursorY = (int) boundingBox.y + y;
					if(!Clay_PointIsInsideRect((Clay_Vector2) { .x = cursorX, .y = cursorY }, scissorBox)) {
						continue;
					}

					Console_MoveCursor(cursorX, cursorY);
					putchar(text.chars[x]);
				}
				Console_Reset();
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
				scissorBox = boundingBox;
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
				scissorBox = fullWindow;
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
				Clay_RectangleRenderData config = renderCommand->renderData.rectangle;
				Console_DrawRectangle(
					(int)boundingBox.x,
					(int)boundingBox.y,
					(int)boundingBox.width,
					(int)boundingBox.height,
					config.backgroundColor,
					scissorBox, ' ');
				break;
			}
			/*case CLAY_RENDER_COMMAND_TYPE_BORDER: {
				Clay_BorderRenderData config = renderCommand->renderData.border;
				// Left border
				if (config.width.left > 0) {
					Console_DrawBorder(
						(int)(boundingBox.x),
						(int)(boundingBox.y),
						(int)config.width.left,
						(int)(boundingBox.height),
						config.color,
						scissorBox,
						'|');
				}
				// Right border
				if (config.width.right > 0) {
					Console_DrawBorder(
						(int)(boundingBox.x + boundingBox.width - config.width.right),
						(int)(boundingBox.y),
						(int)config.width.right,
						(int)(boundingBox.height),
						config.color,
						scissorBox,
						'|');
				}
				// Top border
				if (config.width.top > 0) {
					Console_DrawBorder(
						(int)(boundingBox.x),
						(int)(boundingBox.y),
						(int)(boundingBox.width),
						(int)config.width.top,
						config.color,
						scissorBox,
						'-');

				}
				// Bottom border
				if (config.width.bottom > 0) {
					Console_DrawBorder(
						(int)(boundingBox.x),
						(int)(boundingBox.y + boundingBox.height - config.width.bottom),
						(int)(boundingBox.width),
						(int)config.width.bottom,
						config.color,
						scissorBox,
						'-');
				}
				break;
			}*/
			case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				break;
			default: {
				printf("Error: unhandled render command.");
#ifdef CLAY_OVERFLOW_TRAP
				raise(SIGTRAP);
#endif
				exit(1);
			}
		}
	}
	Clay_PointerData pd = Clay_GetPointerState();
	Console_MoveCursor((int)pd.position.x, (int)pd.position.y);  // TODO make the user not be able to write
	if (pd.state == CLAY_POINTER_DATA_PRESSED){
		printf("▃");
	}else if (pd.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME){
		printf("▅");
	}else {
		printf("▒");
	}
}
