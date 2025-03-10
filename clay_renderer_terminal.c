#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#ifdef CLAY_OVERFLOW_TRAP
#include "signal.h"
#endif

static inline void Console_MoveCursor(int x, int y) {
	printf("\033[%d;%dH", y+1, x+1);
}

bool Clay_PointIsInsideRect(Clay_Vector2 point, Clay_BoundingBox rect) {
	// TODO this function is a copy of Clay__PointIsInsideRect but that one is internal, I don't know if we want
	// TODO to expose Clay__PointIsInsideRect

	return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

static inline void Console_DrawBorder(int x0, int y0, int width, int height, Clay_Color color, Clay_BoundingBox scissorBox, char character) {
	for (int y = y0; y < (y0+height); y++) {
		for (int x = x0; x < (x0+width); x++) {
			if(!Clay_PointIsInsideRect((Clay_Vector2) { .x = x, .y = y }, scissorBox)) {
				continue;
			}

			Console_MoveCursor(x, y);
			if ((x == x0 && y == y0) || (x == (x0+width-1) && y == (y0+height-1)))
			{
				putchar('+');
			}else{
				putchar(character);
			}
		}
	}
}

static inline void Console_DrawRectangle(int x0, int y0, int width, int height, Clay_Color color, Clay_BoundingBox scissorBox) {
	return;
	for (int y = y0; y < height; y++) {
		for (int x = x0; x < width; x++) {
			if(!Clay_PointIsInsideRect((Clay_Vector2) { .x = x, .y = y }, scissorBox)) {
				continue;
			}

			Console_MoveCursor(x, y);
			// TODO there are only two colors actually drawn, the background and white
			if (color.r < 64 || color.g < 64 || color.b < 64 || color.a < 64) {
				printf(" ");
			} else if (color.r < 128 || color.g < 128 || color.b < 128 || color.a < 128) {
				printf("░");
			} else if (color.r < 192 || color.g < 192 || color.b < 192 || color.a < 192) {
				printf("▒");
			} else {
				printf("█");
			}
		}
	}
}

#define MAX(x, y) x > y ? x : y

static inline Clay_Dimensions Console_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
	Clay_Dimensions textSize = { 0 };
	// TODO this function is very wrong, it measures in characters, I have no idea what is the size in pixels
	float maxTextWidth = 0.0f;
	float lineTextWidth = 0;

	float textHeight = 1;

	for (int i = 0; i < text.length; ++i)
	{
		if (text.chars[i] == '\n') {
			maxTextWidth = MAX(maxTextWidth, lineTextWidth);
			lineTextWidth = 0;
			textHeight++;
			continue;
		}
		lineTextWidth++;
	}

	maxTextWidth = MAX(maxTextWidth, lineTextWidth);

	textSize.width = maxTextWidth;
	textSize.height = textHeight;

	return textSize;
}

void Clay_Console_Render(Clay_RenderCommandArray renderCommands, int width, int height)
{
	printf("\033[H\033[J"); // Clear

	const Clay_BoundingBox fullWindow = {
		.x = 0,
		.y = 0,
		.width = (float) width,
		.height = (float) height,
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
				int y = 0;
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
					scissorBox);
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_BORDER: {
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
			}
			default: {
				printf("Error: unhandled render command.");
#ifdef CLAY_OVERFLOW_TRAP
				raise(SIGTRAP);
#endif
				exit(1);
			}
		}
	}

	Console_MoveCursor(-1, -1);  // TODO make the user not be able to write
}
