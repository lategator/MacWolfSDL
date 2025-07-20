#include "Burger.h"
#include "WolfDef.h"
#include "SDLWolf.h"
#include <string.h>
#include <ctype.h>

typedef enum {
	WS_SELECTED = 1,
	WS_FOCUSED = 2,
} widgetstate_t;

typedef struct widgetclass_t widgetclass_t;
typedef struct widget_t widget_t;
struct widgetclass_t {
	void (*render)(widget_t *, widgetstate_t, void *);
	void (*click)(widget_t *, short, short, void *);
};

struct widget_t {
	widgetclass_t *class_;
	Rect rect;
	void *ptr;
};

typedef struct {
	SDL_Surface *pic;
	char *name;
	char *path;
} scenario_t;

static LongWord MenuPosY = 0;
static LongWord MenuScrollY = 0;
static LongWord ScenarioCount = 0;
static SDL_Surface *MenuBG = NULL;
static scenario_t *Scenarios = NULL;
static char *NextScenarioPath = NULL;
char *ScenarioPath = NULL;
int SelectedMenu = -1;

extern char *SaveFileName;

#define RectShrink(r,o) ((Rect){(r)->top+(o), (r)->left+(o), (r)->bottom-(o), (r)->right-(o)})
#define RectOff(r,x,y) ((Rect){(r)->top+(y), (r)->left+(x), (r)->bottom+(y), (r)->right+(x)})
#define RectMod(R,l,t,r,b) ((Rect){(R)->top+(t), (R)->left+(l), (R)->bottom+(b), (R)->right+(r)})
#define FRect(r) ((SDL_FRect){(r)->left, (r)->top, (r)->right - (r)->left, (r)->bottom - (r)->top})
#define FRectShrink(r,o) ((SDL_FRect){(r)->left+(o), (r)->top+(o), (r)->right - (r)->left-(o)*2, (r)->bottom - (r)->top-(o)*2})

static inline Boolean RectContains(const Rect *R, short X, short Y)
{
	return X >= R->left && X < R->right && Y >= R->top && Y < R->bottom;
}

static inline void DrawFilledFrame(const Rect *R)
{
	SDL_FRect FR = FRect(R);
	SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
	SDL_RenderFillRect(SdlRenderer,&FR);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderRect(SdlRenderer,&FR);
}

static inline void DrawDropShadow(const Rect *R)
{
	SDL_SetRenderDrawBlendMode(SdlRenderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 128);
	SDL_RenderFillRects(SdlRenderer,(SDL_FRect[2]){
		{R->left + 3, R->bottom, R->right-R->left, 3},
		{R->right, R->top + 3, 3, R->bottom-R->top-3}}, 2);
	SDL_SetRenderDrawBlendMode(SdlRenderer, SDL_BLENDMODE_NONE);
}

static void RenderWidgets(widget_t *Widgets, int N, int Selected, int Focused, void *Data)
{
	widgetstate_t State;
	int Index;
	for (Index = 0; Index < N; Index++, Widgets++) {
		if (Widgets->class_->render) {
			State = 0;
			if (Index == Selected)
				State |= WS_SELECTED;
			if (Index == Focused)
				State |= WS_FOCUSED;
			Widgets->class_->render(Widgets, State, Data);
		}
	}
}

static int ClickWidgets(widget_t *Widgets, int N, short X, short Y, void *Data)
{
	int Index;
	for (Index = 0; Index < N; Index++, Widgets++) {
		if (RectContains(&Widgets->rect, X, Y)) {
			if (Widgets->class_->click)
				Widgets->class_->click(Widgets, X, Y, Data);
			return Index;
		}
	}
	return -1;
}

typedef struct {
	const char *name;
	widget_t *entries;
	Word n_entries;
	Rect rect;
	int selected;
} menu_t;

typedef struct {
	const char *name;
	Boolean (*getvalue)(widget_t *, void *);
	Boolean disabled;
} menuitem_t;

typedef struct {
	LongWord *pos;
	LongWord *max;
} scrollbar_t;

static void ButtonRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	DrawFilledFrame(&Widget->rect);
	if (State & WS_FOCUSED)
		SDL_RenderRects(SdlRenderer, (SDL_FRect[3]){
			FRectShrink(&Widget->rect, -2), FRectShrink(&Widget->rect, -3), FRectShrink(&Widget->rect, -4)}, 3);
	if (State & WS_SELECTED) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 255, 255, 255);
		SDL_RenderFillRect(SdlRenderer, &FRectShrink(&Widget->rect, 2));
	}
	if (Widget->ptr) {
		FontSetColor(BLACK);
		FontSetClip(&Widget->rect);
		SetFontXY(Widget->rect.left + 8, Widget->rect.top + 3);
		DrawAString(Widget->ptr);
	}
}

static void CheckBoxRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	Rect *R = &Widget->rect;
	Rect Box ={R->top+1, R->left, R->top+13, R->left+12};

	DrawFilledFrame(&Box);
	if (State & WS_FOCUSED)
		SDL_RenderRects(SdlRenderer, (SDL_FRect[3]){
			FRectShrink(&Box, -2), FRectShrink(&Box, -3), FRectShrink(&Box, -4)}, 3);
	if (State & WS_SELECTED) {
		SDL_RenderLine(SdlRenderer, Box.left, Box.top, Box.right-1, Box.bottom-1);
		SDL_RenderLine(SdlRenderer, Box.left, Box.bottom-1, Box.right-1, Box.top);
	}
	if (Widget->ptr) {
		FontSetColor(BLACK);
		FontSetClip(&Widget->rect);
		SetFontXY(Widget->rect.left + 17, Widget->rect.top);
		DrawAString(Widget->ptr);
	}
}

static void TextEntryRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	if (State & WS_SELECTED) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 255, 255, 255);
		SDL_RenderFillRect(SdlRenderer, &FRect(&Widget->rect));
	}
	if (Widget->ptr) {
		FontSetColor(BLACK);
		FontSetClip(&Widget->rect);
		SetFontXY(Widget->rect.left + 3, Widget->rect.top + 1);
		DrawAString(Widget->ptr);
	}
}

static void MenuBarItemRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	menu_t *Menu = Widget->ptr;

	if (State & WS_SELECTED) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderFillRect(SdlRenderer,&FRect(&Widget->rect));
		DrawFilledFrame(&Menu->rect);
		DrawDropShadow(&Menu->rect);
		RenderWidgets(Menu->entries, Menu->n_entries, Menu->selected, -1, Data);
		FontSetColor(WHITE2);
	} else {
		FontSetColor(BLACK);
	}
	FontSetClip(&Widget->rect);
	SetFontXY(Widget->rect.left + 10, Widget->rect.top + 2);
	DrawAString(Menu->name);
}

static void MenuItemRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	menuitem_t *Item = Widget->ptr;
	short X = Widget->rect.left;
	short Y = Widget->rect.top;
	Byte Color;

	if (Item->disabled) {
		State = 0;
		Color = 0x17;
	} else if (State & WS_SELECTED) {
		Color = WHITE2;
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderFillRect(SdlRenderer,&FRect(&Widget->rect));
	} else {
		Color = BLACK;
	}
	if (Item->getvalue && Item->getvalue(Widget, Data)) {
		if (State & WS_SELECTED)
			SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
		else
			SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		for (int y = 0; y < 2; y++)
			SDL_RenderLines(SdlRenderer, (SDL_FPoint[3]){
				{X+3, Y+8+y}, {X+5, Y+10+y}, {X+11, Y+4+y},
			}, 3);
	}
	FontSetColor(Color);
	FontSetClip(&Widget->rect);
	SetFontXY(X + 15, Y + 1);
	DrawAString(Item->name);
}

static void MenuSeparatorRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	float Y;

	Y = Widget->rect.top + ((Widget->rect.bottom - Widget->rect.top)>>1);
	SDL_SetRenderDrawColor(SdlRenderer, 171, 171, 171, 255);
	SDL_RenderLine(SdlRenderer, Widget->rect.left, Y, Widget->rect.right-1, Y);
}

static void VScrollBarRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	scrollbar_t *Scroll = Widget->ptr;
	Rect *R = &Widget->rect;
	LongWord PHeight = R->bottom - R->top;
	LongWord Height = PHeight/16;
	LongWord Max = *Scroll->max;
	short X1 = R->left+1;
	short X2 = R->right-2;
	short Y;
	int i;
	SDL_Vertex Verts[7];

	static const Byte Arrow[8][2] = {
		{7, 2}, {12, 7}, {9, 7}, {9, 11}, {4, 11}, {4, 7}, {1, 7}, {6, 2},
	};
	static const int ArrowIndexes[9] = {0, 1, 6, 5, 2, 4, 2, 3, 4};

	if (Max > Height) {
		SDL_SetRenderDrawColor(SdlRenderer, 171, 171, 171, 255);
		SDL_RenderFillRect(SdlRenderer, &(SDL_FRect){R->left, R->top+16, R->right - R->left, R->bottom - R->top - 32});
	}
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderRect(SdlRenderer,&FRect(R));
	Y = R->top + 15;
	SDL_RenderLine(SdlRenderer, X1, Y, X2, Y);
	Y = R->bottom - 16;
	SDL_RenderLine(SdlRenderer, X1, Y, X2, Y);

	if (Max <= Height) {
		SDL_SetRenderDrawColor(SdlRenderer, 171, 171, 171, 255);
	} else {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		Y = R->top + 1;
		for (i = 0; i < 7; i++) {
			Verts[i].position.x = Arrow[i][0]+X1;
			Verts[i].position.y = Arrow[i][1]+Y;
			Verts[i].color = (SDL_FColor){ 0, 0, 0, 255 };
		}
		SDL_RenderGeometry(SdlRenderer, NULL, Verts, 7, ArrowIndexes, 9);
		Y = R->bottom-15+1;
		for (i = 0; i < 7; i++) {
			Verts[i].position.x = Arrow[i][0]+X1;
			Verts[i].position.y = 13-Arrow[i][1]+Y;
		}
		SDL_RenderGeometry(SdlRenderer, NULL, Verts, 7, ArrowIndexes, 9);
	}

	Y = R->top+1;
	for (i = 0; i < 7; i++) {
		SDL_RenderLine(SdlRenderer,
				Arrow[i][0]+X1,
				Arrow[i][1]+Y,
				Arrow[i+1][0]+X1,
				Arrow[i+1][1]+Y);
	}
	Y = R->bottom-15;
	for (i = 0; i < 7; i++) {
		SDL_RenderLine(SdlRenderer,
				Arrow[i][0]+X1,
				13-Arrow[i][1]+Y,
				Arrow[i+1][0]+X1,
				13-Arrow[i+1][1]+Y);
	}

	if (Max > Height) {
		Y = R->top + 15 + *Scroll->pos / (float) (Max - Height) * (PHeight - 48 + 2);
		DrawFilledFrame(&(Rect){Y, R->left, Y+16, R->right});
	}
}

static void VScrollBarClick(widget_t *Widget, short X, short Y, void *Data)
{
	scrollbar_t *Scroll = Widget->ptr;
	Rect *R = &Widget->rect;
	LongWord PHeight = R->bottom - R->top;
	LongWord Height = PHeight/16;
	LongWord Max = *Scroll->max;
	LongWord Pos;

	Max = *Scroll->max;
	if (Max <= Height)
		return;
	Pos = *Scroll->pos;
	if (Y < R->top + 16) {
		if (Pos > 0)
			Pos--;
	} else if (Y >= R->bottom - 16) {
		if (Pos < Max - Height)
			Pos++;
	} else {
		Pos = SDL_roundf((Y - R->top - 15) / (float) (PHeight - 48 + 2) * (Max - Height));
		if (Pos > Max - Height)
			Pos = Max - Height;
		if (Pos < 0)
			Pos = 0;
	}
	*Scroll->pos = Pos;
}

static widgetclass_t ButtonClass = { ButtonRender, NULL };
static widgetclass_t CheckBoxClass = { CheckBoxRender, NULL };
static widgetclass_t TextEntryClass = { TextEntryRender, NULL };
static widgetclass_t MenuBarItemClass = { MenuBarItemRender, NULL };
static widgetclass_t MenuItemClass = { MenuItemRender, NULL };
static widgetclass_t MenuSeparatorClass = { MenuSeparatorRender, NULL };
static widgetclass_t VScrollBarClass = { VScrollBarRender, VScrollBarClick };
static widgetclass_t EmptyClass = { NULL, NULL };

static SDL_EnumerationResult MakeScenarioList(void *UserData, const char *Dir, const char *Name)
{
	char *NameBuf;
	char *Path;
	char *s;
	const char *t;
	ResourceFile *Rp;
	scenario_t *Scenario;
	size_t NameLen;
	char c;

	NameLen = strlen(Name);
	NameBuf = AllocSomeMem(NameLen+1);
	for (s = NameBuf, t = Name; *t; s++, t++) {
		c = *t;
		if (isspace(c))
			*s = ' ';
		else if (c < ' ')
			*s = '_';
		else
			*s = c;
	}
	*s = '\0';
	if (NameLen >= 5 && !SDL_strcasecmp(".rsrc", &NameBuf[NameLen-5]))
		NameBuf[NameLen-5] = '\0';

	Path = AllocSomeMem(strlen(Dir) + strlen(Name) + 1);
	stpcpy(stpcpy(Path, Dir), Name);
	Rp = LoadResourceForkFile(Path);
	if (!Rp) {
		SDL_free(NameBuf);
		SDL_free(Path);
		return SDL_ENUM_CONTINUE;
	}

	ScenarioCount++;
	Scenarios = SDL_realloc(Scenarios, ScenarioCount * sizeof(scenario_t));
	if (!Scenarios) BailOut("Out of memory");
	Scenario = &Scenarios[ScenarioCount-1];
	Scenario->path = Path;
	Scenario->name = NameBuf;
	Scenario->pic = LoadPict(Rp, 1);

	ReleaseResources(Rp);
	return SDL_ENUM_CONTINUE;
}

#define SCENARIOLISTX 14
#define SCENARIOLISTY 82
#define SCENARIOLISTW 325
#define SCENARIOLISTH 176
static const Rect ScenarioListRect = {SCENARIOLISTY, SCENARIOLISTX, SCENARIOLISTY+SCENARIOLISTH, SCENARIOLISTX+SCENARIOLISTW};
static const LongWord ScenariosItemHeight = SCENARIOLISTH/16;
static widget_t ScenarioWidgets[2] = {
	{&VScrollBarClass,
	{SCENARIOLISTY, SCENARIOLISTX+SCENARIOLISTW-1, SCENARIOLISTY+SCENARIOLISTH, SCENARIOLISTX+SCENARIOLISTW+15},
	&(scrollbar_t){&MenuScrollY, &ScenarioCount}},
	{&ButtonClass, {288, 155, 308, 213}, "Cancel"}
};

extern void BlitScenarioSelect(SDL_Surface* BG, SDL_Surface* Pic, const SDL_Rect* Rect);

static void DrawScenarioList(void)
{
	int i;
	Word Y;
	int Max;
	SDL_FRect FR;
	SDL_Surface *ScenarioPic = NULL;

	if (ScenarioCount)
		ScenarioPic = Scenarios[MenuPosY].pic;
	BlitScenarioSelect(MenuBG, ScenarioPic, &(SDL_Rect){231, 17, 96, 64});
	StartUIOverlay();
	if (!ScenarioPic) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderFillRect(SdlRenderer, &(SDL_FRect){231, 17, 96, 64});
	}
	DrawFilledFrame(&RectMod(&ScenarioListRect,0,0,16,0));
	if (!MenuBG) {
		FontSetColor(WHITE2);
		SetFontXY(48, 39);
		FontSetClip(NULL);
		DrawAString("Which scenario?");
	}
	FontSetColor(BLACK);
	FontSetClip(&ScenarioListRect);
	Y = ScenarioListRect.top;
	Max = MenuScrollY + ScenariosItemHeight;
	Max = Max < ScenarioCount ? Max : ScenarioCount;
	for (i = MenuScrollY; i < Max; i++) {
		SetFontXY(17, Y+1);
		DrawAString(Scenarios[i].name);
		Y += 16;
	}
	RenderWidgets(ScenarioWidgets, 2, -1, -1, NULL);
	if (ScenarioCount) {
		FR.x = ScenarioListRect.left+1;
		FR.y = ScenarioListRect.top + (MenuPosY - MenuScrollY) * 16;
		FR.w = ScenarioListRect.right-ScenarioListRect.left-2;
		FR.h = 16;
		SDL_SetRenderDrawColor(SdlRenderer, 0, 255, 255, 255);
		SDL_RenderFillRect(SdlRenderer, &FR);
	}
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderRect(SdlRenderer, &FRect(&ScenarioListRect));
	EndUIOverlay();
}

static int CompareScenario(const void *a, const void *b)
{
	return SDL_strcasecmp(((scenario_t*)a)->name,((scenario_t*)b)->name);
}

Word ChooseScenario(void)
{
	int i;
	int Click = 0;
	int oldmousex, oldmousey;
	Boolean Redraw;
	Word RetVal = FALSE;

	EnumerateLevels(MakeScenarioList, NULL);
	if (!ScenarioCount)
			BailOut("No scenario files found!\n\n"
					"At least one scenario is required to play the game.\n"
					"The base scenarios can be copied from the 'Levels' folder of an installed Wolfenstein 3D Third Encounter for Macintosh Classic.\n"
					"The file format can be any of: MacBinary II, AppleSingle, AppleDouble, or a raw resource fork.\n\n"
					"Scenarios can be placed in either of these folders:\n\n%sLevels\n%sLevels",
					SDL_GetBasePath(), PrefPath());
	SDL_qsort(Scenarios, ScenarioCount, sizeof(scenario_t), CompareScenario);
	if (ScenarioPath) {
		for (i = 0; i < ScenarioCount; i++) {
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_APPLE)
			if (!SDL_strcasecmp(Scenarios[i].path, ScenarioPath)) {
#else
			if (!strcmp(Scenarios[i].path, ScenarioPath)) {
#endif
				MenuPosY = i;
				break;
			}
		}
	} else {
		MenuPosY = 0;
	}
	MenuScrollY = MenuPosY >= ScenariosItemHeight ? MenuPosY - ScenariosItemHeight + 1 : MenuPosY;
	ResizeGameWindow(369, 331);
	ClearTheScreen(WHITE);
	ClearFrameBuffer();
	MenuBG = LoadPict(MainResources, 129);
	DrawScenarioList();
	ReadMenuJoystick();
	for (;;) {
		if (!Click)
			WaitTick();
		oldmousex = mousex;
		oldmousey = mousey;
		Click = ReadMenuJoystick();
		if (joystick1 & JOYPAD_B) {
			PlaySound(SND_OK);
			break;
		}
		Redraw = FALSE;
		if (joystick1 & JOYPAD_UP) {
			if (MenuPosY > 0) {
				if (MenuScrollY == MenuPosY)
					MenuScrollY--;
				PlaySound(SND_GUNSHT);
				MenuPosY--;
				Redraw = TRUE;
			}
		}
		if (joystick1 & JOYPAD_DN) {
			if (MenuPosY < ScenarioCount - 1) {
				if (MenuScrollY + ScenariosItemHeight - 1 == MenuPosY)
					MenuScrollY++;
				PlaySound(SND_GUNSHT);
				MenuPosY++;
				Redraw = TRUE;
			}
		}
		for (i = mousewheel; i > 0; i--) {
			if (MenuScrollY == 0)
				break;
			if (MenuScrollY + ScenariosItemHeight - 1 == MenuPosY)
				MenuPosY--;
			MenuScrollY--;
			Redraw = TRUE;
		}
		for (i = mousewheel; i < 0; i++) {
			if (ScenarioCount <= ScenariosItemHeight || MenuScrollY >= ScenarioCount - ScenariosItemHeight)
				break;
			if (MenuScrollY == MenuPosY)
				MenuPosY++;
			MenuScrollY++;
			Redraw = TRUE;
		}
		if (Click == 1) {
			i = ClickWidgets(ScenarioWidgets, 2, mousex, mousey, NULL);
			if (i == 0) {
				if (MenuPosY < MenuScrollY)
					MenuPosY = MenuScrollY;
				else if (MenuPosY >= MenuScrollY + ScenariosItemHeight)
					MenuPosY = MenuScrollY + ScenariosItemHeight;
				Redraw = TRUE;
			} else if (i == 1)
				break;
		}
		if (RectContains(&RectShrink(&ScenarioListRect, 1), mousex, mousey)) {
			if (oldmousex != mousex || oldmousey != mousey) {
				i = MenuScrollY + (mousey - ScenarioListRect.top) / 16;
				if (i != MenuPosY && i < ScenarioCount && i >= 0) {
					MenuPosY = i;
					Redraw = TRUE;
				}
			}
			if (Click == 1) {
				PlaySound(SND_MENU);
				RetVal = TRUE;
				break;
			}
		}
		if (joystick1 & JOYPAD_A) {
			PlaySound(SND_MENU);
			RetVal = TRUE;
			break;
		}
		if (Redraw)
			DrawScenarioList();
	}
	if (MenuBG) {
		SDL_DestroySurface(MenuBG);
		MenuBG = NULL;
	}

	if (Scenarios) {
		for (i = 0; i < ScenarioCount; i++) {
			SDL_free(Scenarios[i].name);
			if (i == MenuPosY)
				NextScenarioPath = Scenarios[i].path;
			else
				SDL_free(Scenarios[i].path);
			if (Scenarios[i].pic)
				SDL_DestroySurface(Scenarios[i].pic);
		}
		SDL_free(Scenarios);
	}
	Scenarios = NULL;
	ScenarioCount = 0;

	return RetVal;
}

static widget_t DifficultyWidgets[5] = {
	{&EmptyClass, {63, 73, 139, 128}},
	{&EmptyClass, {63, 228, 139, 283}},
	{&EmptyClass, {165, 73, 241, 128}},
	{&EmptyClass, {165, 228, 241, 283}},
	{&ButtonClass, {288, 155, 308, 213}, "Cancel"},
};

static void DrawGameDiff(void)
{
	Rect *R;

	if (MenuBG)
		BlitSurface(MenuBG, NULL);
	StartUIOverlay();
	if (MenuPosY < 4) {
		SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
		R = &DifficultyWidgets[MenuPosY].rect;
		SDL_RenderRects(SdlRenderer,(SDL_FRect[3]){FRect(R), FRectShrink(R, 1), FRectShrink(R, 2)}, 3);
	}
	ButtonRender(&DifficultyWidgets[4], MenuPosY == 4, NULL);
	EndUIOverlay();
}

Word ChooseGameDiff(void)
{
	Boolean Click = 0;
	int oldmousex, oldmousey;
	int i;
	Word RetVal = FALSE;

	PlaySound(SND_MENU);
TryAgain:
	if (!ChooseScenario()) {
		if (NextScenarioPath) {
			SDL_free(NextScenarioPath);
			NextScenarioPath = NULL;
		}
		if (playstate == EX_STILLPLAYING || playstate == EX_AUTOMAP) {
			GameViewSize = NewGameWindow(GameViewSize);
TryIt:
			if (!StartupRendering(GameViewSize)) {	/* Set the size of the game screen */
				ReleaseScalers();
				if (!GameViewSize) {
					BailOut("Failed to set up video mode");
				}
				--GameViewSize;
				goto TryIt;
			}
			if (playstate==EX_STILLPLAYING) {
				SetAPalette(rGamePal);			/* Reset the game palette */
				RedrawStatusBar();		/* Redraw the lower area */
			}
		} else {
			NewGameWindow(2);
		}
		return FALSE;
	}

	MenuScrollY = MenuPosY = difficulty;
	ResizeGameWindow(369, 331);
	ClearTheScreen(WHITE);
	ClearFrameBuffer();
	MenuBG = LoadPict(MainResources, 128);
	DrawGameDiff();
	ReadMenuJoystick();
	for (;;) {
		if (!Click)
			WaitTick();
		oldmousex = mousex;
		oldmousey = mousey;
		Click = ReadMenuJoystick();
		if (joystick1 & JOYPAD_B)
			goto TryAgain;
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = MenuPosY & 1;
		if (joystick1 & JOYPAD_DN)
			MenuScrollY = MenuPosY | 2;
		if (joystick1 & JOYPAD_LFT)
			MenuScrollY = MenuPosY & 2;
		if (joystick1 & JOYPAD_RGT)
			MenuScrollY = MenuPosY | 1;
		if (MenuScrollY != MenuPosY)
			PlaySound(SND_GUNSHT);
		if (oldmousex != mousex || oldmousey != mousey || Click == 1) {
			i = ClickWidgets(DifficultyWidgets, 5, mousex, mousey, NULL);
			if (i == 4) {
				if (Click == 1) {
					PlaySound(SND_OK);
					goto TryAgain;
				}
			} else if (i >= 0) {
				MenuScrollY = i;
				if (Click == 1) {
					PlaySound(SND_OK);
					break;
				}
			}
		}
		if (joystick1 & JOYPAD_A) {
			PlaySound(SND_OK);
			break;
		}
		if (MenuScrollY != MenuPosY) {
			MenuPosY = MenuScrollY;
			DrawGameDiff();
		}
	}
	if (MenuBG) {
		SDL_DestroySurface(MenuBG);
		MenuBG = NULL;
	}

	difficulty = MenuPosY;
	if (NextScenarioPath) {
		if (ScenarioPath)
			SDL_free(ScenarioPath);
		ScenarioPath = NextScenarioPath;
		ClearFrameBuffer();
		RenderScreen();
		RetVal = MountMapFile(ScenarioPath);
		LoadMapSetData();
		playstate = EX_NEWGAME;	/* Start a new game */
		SaveFileName = 0;		/* Zap the save game name */
	}
	return RetVal;
}

static Boolean GetFullScreen(widget_t*_W,void*_D) { return FullScreen; }
static Boolean GetSoundEnabled(widget_t*_W,void*_D) { return (SystemState & SfxActive) != 0; }
static Boolean GetMusicEnabled(widget_t*_W,void*_D) { return (SystemState & MusicActive) != 0; }
static Boolean GetMouseEnabled(widget_t*_W,void*_D) { return MouseEnabled; }
static Boolean GetSlowDown(widget_t*_W,void*_D) { return SlowDown; }
static Boolean GetPaused(widget_t*_W,void*_D) { return TRUE; }

static menu_t FileMenu = {"File", (widget_t[7]){
	{&MenuItemClass, {20, 13, 36, 144}, &(menuitem_t){"New Game..."}},
	{&MenuItemClass, {36, 13, 52, 144}, &(menuitem_t){"Open"}},
	{&MenuSeparatorClass, {52, 13, 68, 144}},
	{&MenuItemClass, {68, 13, 84, 144}, &(menuitem_t){"Save"}},
	{&MenuItemClass, {84, 13, 100, 144}, &(menuitem_t){"Save As..."}},
	{&MenuSeparatorClass, {100, 13, 116, 144}},
	{&MenuItemClass, {116, 13, 132, 144}, &(menuitem_t){"Quit"}},
}, 7, {19, 12, 133, 145}, -1};
static menu_t OptionsMenu = {"Options", (widget_t[8]){
	{&MenuItemClass, {20, 54, 36, 233}, &(menuitem_t){"Full Screen", GetFullScreen}},
	{&MenuItemClass, {36, 54, 52, 233}, &(menuitem_t){"Sound", GetSoundEnabled}},
	{&MenuItemClass, {52, 54, 68, 233}, &(menuitem_t){"Music", GetMusicEnabled}},
	{&MenuItemClass, {68, 54, 84, 233}, &(menuitem_t){"Set Screen size..."}},
	{&MenuItemClass, {84, 54, 100, 233}, &(menuitem_t){"Speed Governor", GetSlowDown}},
	{&MenuItemClass, {100, 54, 116, 233}, &(menuitem_t){"Mouse Control", GetMouseEnabled}},
	{&MenuItemClass, {116, 54, 132, 233}, &(menuitem_t){"Pause", GetPaused}},
	{&MenuItemClass, {132, 54, 148, 233}, &(menuitem_t){"Configure Keyboard"}},
}, 8, {19, 53, 149, 234}, -1};
static widget_t MenuBar[2] = {
	{&MenuBarItemClass, {1, 12, 19, 53},&FileMenu},
	{&MenuBarItemClass, {1, 53, 19, 120},&OptionsMenu},
};

void DrawMainMenu(int SelectedMenu)
{
	SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
	SDL_RenderFillRect(SdlRenderer,&(SDL_FRect){0, 0, SCREENWIDTH, 19});
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderLine(SdlRenderer, 0, 19, SCREENWIDTH, 19);
	RenderWidgets(MenuBar, 2, SelectedMenu, -1, NULL);
}

int DoMenuCommand(int Menu, int Item)
{
	switch (Menu) {
	case 0:				/* File menu */
		switch (Item) {
		case 0:
			if (ChooseGameDiff()) {		/* Choose level of difficulty */
				return EX_NEWGAME;
			}
			return -2;
		case 1:
			if (ChooseLoadGame()) {		/* Choose a game to load */
				if (LoadGame()) {				/* Load the game into memory */
					return EX_LOADGAME;	/* Restart a loaded game */
				}
			}
			break;
		case 3:
			if (!SaveFileName)			/* Save the file automatically? */
				ChooseSaveGame();		/* Select a save game name */
			if (SaveFileName)
				SaveGame();				/* Save it */
			break;
		case 4:
			if (ChooseSaveGame()) {		/* Select a save game name */
				SaveGame();				/* Save it */
			}
			break;
		case 6:
			GoodBye();	/* Try to quit */
			break;
		}
		break;
	case 1:
		switch (Item) {
		case 0:
			FullScreen^=1;
			SDL_SetWindowFullscreen(SdlWindow, FullScreen);
			UpdateVideoSettings();
			RenderScreen();
			break;
		case 1:
			SystemState^=SfxActive;				/* Sound on/off flags */
			if (!(SystemState&SfxActive)) {
				PlaySound(0);			/* Turn off all existing sounds */
			}
			break;
		case 2:
			SystemState^=MusicActive;			/* Music on/off flags */
			if (SystemState&MusicActive) {
				PlaySong(KilledSong);		/* Restart the music */
			} else {
				PlaySong(0);	/* Shut down the music */
			}
			break;
		case 3:
			return -3;
		case 4:
			SlowDown^=1;		/* Toggle the slow down flag */
			break;
		case 5:
			MouseEnabled = (!MouseEnabled);	/* Toggle the cursor */
			break;
		case 6:			/* Unpause */
			return -1;
		case 7:			/* Keyboard control window */
			return -4;
		}
		SavePrefs();
		break;
	}
	return 0;
}

static const Rect VideoDialogRect = {0, 0, 166, 318};
static const Rect ScreenButtonRect = {0, 0, 20, 88};
static const Rect ScreenFilterRect = {0, 0, 14, 88};
static const Rect ScreenDoneRect = {0, 0, 20, 50};

static widget_t ScreenButtons[9] = {
	{&ButtonClass, {0}, "320 x 200"},
	{&ButtonClass, {0}, "512 x 384"},
	{&ButtonClass, {0}, "640 x 400"},
	{&ButtonClass, {0}, "640 x 480"},
	{&ButtonClass, {0}, "Integer"},
	{&ButtonClass, {0}, "Scale"},
	{&ButtonClass, {0}, "Stretch"},
	{&CheckBoxClass, {0}, "Filtering"},
	{&ButtonClass, {0}, "Done"},
};


static void InitVideoDialog(void)
{
	int X, Y;

	X = (SCREENWIDTH-VideoDialogRect.right)/2+50;
	Y = (SCREENHEIGHT-VideoDialogRect.bottom)/2+50;
	MenuScrollY = MenuPosY = GameViewSize;

	ScreenButtons[0].rect = RectOff(&ScreenButtonRect, X,	 Y);
	ScreenButtons[1].rect = RectOff(&ScreenButtonRect, X+124, Y);
	ScreenButtons[2].rect = RectOff(&ScreenButtonRect, X,	 Y+26);
	ScreenButtons[3].rect = RectOff(&ScreenButtonRect, X+124, Y+26);

	X -= 24;
	Y += 60;

	ScreenButtons[4].rect = RectOff(&ScreenButtonRect, X,	 Y);
	ScreenButtons[5].rect = RectOff(&ScreenButtonRect, X+94, Y);
	ScreenButtons[6].rect = RectOff(&ScreenButtonRect, X+188, Y);

	X += 24;
	Y += 30;

	ScreenButtons[7].rect = RectOff(&ScreenFilterRect, X, Y);

	X += 180;
	Y -= 3;

	ScreenButtons[8].rect = RectOff(&ScreenDoneRect, X, Y);
}

static int RunVideoDialog(int Click, Boolean Moved)
{
	int i = -1;
	int Selected = -1;

	if (Click == 3 || (joystick1 & JOYPAD_B))
		return -1;
	if (MenuPosY < 4) {
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = MenuPosY & 1;
		if (joystick1 & JOYPAD_LFT)
			MenuScrollY = MenuPosY & 2;
		if (joystick1 & JOYPAD_RGT)
			MenuScrollY = MenuPosY | 1;
		if (joystick1 & JOYPAD_DN) {
			if (MenuPosY < 2)
				MenuScrollY = MenuPosY | 2;
			else
				MenuScrollY = ScreenScaleMode + 4;
		}
	} else if (MenuPosY >= 4 && MenuPosY < 7) {
		if ((joystick1 & JOYPAD_LFT) && MenuPosY > 4)
			MenuScrollY = MenuPosY - 1;
		if ((joystick1 & JOYPAD_RGT) && MenuPosY < 6)
			MenuScrollY = MenuPosY + 1;
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = GameViewSize;
		else if (joystick1 & JOYPAD_DN)
			MenuScrollY = 7;
	} else if (MenuPosY == 7) {
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = ScreenScaleMode + 4;
		if (joystick1 & JOYPAD_RGT)
			MenuScrollY = 8;
	} else if (MenuPosY == 8) {
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = ScreenScaleMode + 4;
		if (joystick1 & JOYPAD_LFT)
			MenuScrollY = 7;
	}
	if (Moved || Click == 1) {
		i = ClickWidgets(ScreenButtons, 9, mousex, mousey, NULL);
		if (i >= 0) {
			MenuScrollY = i;
			if (Click == 1)
				Selected = i;
		}
	}
	if (joystick1 & JOYPAD_A)
		Selected = MenuScrollY;
	if (Selected >= 0 && Selected < 4) {
		if (GameViewSize!=Selected) {		/* Did you change the size? */
			GameViewSize = Selected;		/* Set the new size */
			if (playstate==EX_STILLPLAYING || playstate==EX_AUTOMAP) {
TryIt:
				GameViewSize = NewGameWindow(GameViewSize);		/* Make a new window */
				if (!StartupRendering(GameViewSize)) {	/* Set the size of the game screen */
					ReleaseScalers();
					if (!GameViewSize) {
						BailOut("Failed to set up video mode");
					}
					--GameViewSize;
					goto TryIt;
				}
				playstate = EX_STILLPLAYING;
				SetAPalette(rGamePal);			/* Reset the game palette */
				RedrawStatusBar();		/* Redraw the lower area */
				RenderView();
				InitVideoDialog();
			}
			SavePrefs();
			return 1;
		}
	} else if (Selected >= 4 && Selected < 7) {
		if (ScreenScaleMode != Selected-4) {
			ScreenScaleMode = Selected-4;
			UpdateVideoSettings();
			SavePrefs();
			return 1;
		}
	} else if (Selected == 7) {
		ScreenFilter ^= 1;
		UpdateVideoSettings();
		SavePrefs();
		return 1;
	} else if (Selected == 8)
		return -1;
	if (MenuScrollY != MenuPosY) {
		MenuPosY = MenuScrollY;
	}
	return 0;
}

static void DrawVideoDialog(void)
{
	int X, Y;

	X = (SCREENWIDTH-VideoDialogRect.right)/2;
	Y = (SCREENHEIGHT-VideoDialogRect.bottom)/2;

	DrawFilledFrame(&RectOff(&VideoDialogRect, X, Y));
	DrawDropShadow(&RectOff(&VideoDialogRect, X, Y));
	FontSetColor(BLACK);
	FontSetClip(NULL);
	SetFontXY(X+51, Y+8);
	DrawAString("What screen size shall you conquer\nthe castle with?");
	RenderWidgets(&ScreenButtons[0], 4, GameViewSize, MenuPosY, NULL);
	RenderWidgets(&ScreenButtons[4], 3, ScreenScaleMode, MenuPosY-4, NULL);
	RenderWidgets(&ScreenButtons[7], 1, (int)ScreenFilter-1, MenuPosY-7, NULL);
	RenderWidgets(&ScreenButtons[8], 1, -1, MenuPosY-8, NULL);
}

static const Rect KeyboardDialogRect = {0, 0, 216, 306};
static const Rect KeysFrameRect = {12, 21, 204, 234};
static const Rect KeyButtonRect = {12, 134, 28, 234};
static const Byte KeyIndexes[12] = {7, 6, 5, 4, 1, 0, 9, 11, 2, 10, 3, 8};
static const char *const KeyNames[12] = {
	"Forward", "Backward", "Turn Left", "Turn Right", "Sidestep Left", "Sidestep Right",
	"Next Weapon", "Shoot", "Sidestep", "Run", "Action", "Auto Map"};
static widget_t KeyButtons[12];
static widget_t KeyOkCancelButtons[2];
static SDL_Keycode TmpKeyBinds[12];
static const Rect KeyResponseButtonRect = {0, 0, 20, 58};

static void InitKeyboardDialog(void)
{
	int X, Y;
	int i;

	X = (SCREENWIDTH-KeyboardDialogRect.right)/2;
	Y = (SCREENHEIGHT-KeyboardDialogRect.bottom)/2;

	MenuScrollY = MenuPosY = 0;
	for (i = 0; i < ARRAYLEN(KeyBinds); i++) {
		KeyButtons[i] = (widget_t) {&TextEntryClass, RectOff(&KeyButtonRect, X, Y+i*16),
			(void*)SDL_GetScancodeName(KeyBinds[KeyIndexes[i]])};
		TmpKeyBinds[i] = KeyBinds[KeyIndexes[i]];
	}
	KeyOkCancelButtons[0] = (widget_t) {&ButtonClass, RectOff(&KeyResponseButtonRect, X+240, Y+84), "Cancel"};
	KeyOkCancelButtons[1] = (widget_t) {&ButtonClass, RectOff(&KeyResponseButtonRect, X+240, Y+116), "Ok"};
}

static int RunKeyboardDialog(int Click)
{
	int i;
	if (Click == 3 || (joystick1 & JOYPAD_B))
		return -1;
	if (MenuPosY >= 12) {
		if (joystick1 & JOYPAD_LFT)
			MenuScrollY = 0;
		if ((joystick1 & JOYPAD_UP) && MenuPosY == 13)
			MenuScrollY = 12;
		if ((joystick1 & JOYPAD_DN) && MenuPosY == 12)
			MenuScrollY = 13;
	} else if (mousewheel) {
		for (i = MenuScrollY; mousewheel; mousewheel -= (mousewheel > 0 ? 1 : -1)) {
			i += (mousewheel > 0 ? -1 : 1);
			if (i < 0) {
				i = 0;
				break;
			} else if (i >= 12) {
				i = 11;
				break;
			}
		}
		MenuScrollY = i;
	}
	if (Click == 1) {
		i = ClickWidgets(KeyButtons, 12, mousex, mousey, NULL);
		if (i >= 0) {
			MenuScrollY = i;
		} else {
			i = ClickWidgets(KeyOkCancelButtons, 2, mousex, mousey, NULL);
			if (i >= 0) {
				MenuPosY = i+12;
				return -1;
			}
		}
	}
	if (MenuScrollY >= 12 && (joystick1 & JOYPAD_A)) {
		MenuPosY = MenuScrollY;
		return -1;
	}
	if (MenuScrollY != MenuPosY) {
		MenuPosY = MenuScrollY;
		return 1;
	}
	return 0;
}

static void DrawKeyboardDialog(void)
{
	int X, Y;
	int i;

	X = (SCREENWIDTH-KeyboardDialogRect.right)/2;
	Y = (SCREENHEIGHT-KeyboardDialogRect.bottom)/2;

	DrawFilledFrame(&RectOff(&KeyboardDialogRect, X, Y));
	DrawDropShadow(&RectOff(&KeyboardDialogRect, X, Y));
	FontSetColor(BLACK);
	FontSetClip(NULL);
	for (i = 0; i < 12; i++) {
		SetFontXY(KeyButtonRect.left+X-110, KeyButtonRect.top+Y+i*16+1);
		DrawAString(KeyNames[i]);
	}
	RenderWidgets(KeyButtons, 12, MenuPosY, -1, NULL);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderRect(SdlRenderer,&FRect(&RectOff(&KeysFrameRect, X, Y)));
	RenderWidgets(KeyOkCancelButtons, 2, -1, MenuPosY-12, NULL);
}

static int GetAKey(void)
{
	SDL_Event event;

	joystick1 = 0;
	mousewheel = 0;

	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (ProcessGlobalEvent(&event)) {
			if (event.type == SDL_EVENT_WINDOW_RESIZED)
				return SDL_MAX_SINT32;
			continue;
		}
		SDL_ConvertEventToRenderCoordinates(SdlRenderer, &event);
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mousewheel += event.wheel.y;
		} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			mousex = event.motion.x;
			mousey = event.motion.y;
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mousex = event.motion.x;
			mousey = event.motion.y;
			return event.button.button;
		} else if (event.type == SDL_EVENT_KEY_DOWN) {
			return -event.key.scancode;
		}
	}
	return 0;
}

#define MENUITEM(widget) ((menuitem_t*)(widget)->ptr)

exit_t PauseMenu(Boolean Shape)
{
	Byte *ShapePtr = NULL;
	int Click = 0;
	int oldmousex, oldmousey;
	int SelectedItem;
	int i;
	menu_t *Menu;
	Boolean Redraw;
	Word OpenDialog = 0;
	exit_t RetVal = EX_LIMBO;
	Boolean Playing;

	Playing = playstate == EX_STILLPLAYING || playstate == EX_AUTOMAP;
	if (Playing)
		PauseSoundMusicSystem();
	MENUITEM(&FileMenu.entries[3])->disabled = !Playing;
	MENUITEM(&FileMenu.entries[4])->disabled = !Playing;
	UngrabMouse();
	if (Shape) {
		ShapePtr = LoadCompressedShape(rPauseShape);
		if (ShapePtr) {
			DrawShape((SCREENWIDTH-224)/2, (MacViewHeight-56)/2, ShapePtr);
			BlitScreen();
		}
	}
	StartUIOverlay();
	DrawMainMenu(SelectedMenu);
	EndUIOverlay();
	for (;;) {
		if (!Click)
			WaitTick();
		oldmousex = mousex;
		oldmousey = mousey;
		if (OpenDialog == 2 && MenuPosY < 12) {
			Click = GetAKey();
			if (Click < 0) {
				i = -Click;
				if (i == SDL_SCANCODE_ESCAPE) {
					joystick1 = JOYPAD_B;
				} else if (i != SDL_SCANCODE_1 && i != SDL_SCANCODE_2 && i != SDL_SCANCODE_3
					&& i != SDL_SCANCODE_4 && i != SDL_SCANCODE_5 && i != SDL_SCANCODE_6) {
					TmpKeyBinds[MenuPosY] = i;
					KeyButtons[MenuPosY].ptr = (void*)SDL_GetScancodeName(i);
					MenuScrollY++;
					if (MenuScrollY == 12)
						MenuScrollY++;
					Redraw = TRUE;
				}
			}
		} else {
			Click = ReadMenuJoystick();
		}
		if (Click == SDL_MAX_SINT32) {
			switch (OpenDialog) {
				case 1: InitVideoDialog(); break;
				case 2: InitKeyboardDialog(); break;
				default: break;
			}
			Redraw = TRUE;
		}
		if (OpenDialog) {
			switch (OpenDialog) {
				case 1: i = RunVideoDialog(Click, oldmousex != mousex || oldmousey != mousey); break;
				case 2: i = RunKeyboardDialog(Click); break;
				default: i = -1; break;
			}
			if (i < 0) {
				if (OpenDialog && ShapePtr) {
					DrawShape((SCREENWIDTH-224)/2, (MacViewHeight-56)/2, ShapePtr);
					BlitScreen();
				}
				if (OpenDialog == 2 && MenuPosY == 13) {
					for (i = 0; i < ARRAYLEN(KeyBinds); i++)
						KeyBinds[KeyIndexes[i]] = TmpKeyBinds[i];
					SavePrefs();
				}
				OpenDialog = 0;
				Redraw = TRUE;
			} else if (i > 0)
				Redraw = TRUE;
		} else {
			Redraw = FALSE;
			if ((joystick1 & JOYPAD_B) || Click == 3)
				break;
			if (joystick1 & JOYPAD_LFT) {
				if (SelectedMenu < 0)
					SelectedMenu = 1;
				else
					SelectedMenu--;
				Redraw = TRUE;
			}
			if (joystick1 & JOYPAD_RGT) {
				if (SelectedMenu >= 1)
					SelectedMenu = -1;
				else
					SelectedMenu++;
				Redraw = TRUE;
			}
			if (joystick1 & JOYPAD_UP)
				mousewheel++;
			if (joystick1 & JOYPAD_DN)
				mousewheel--;
			if (SelectedMenu >= 0 && mousewheel) {
				Menu = MenuBar[SelectedMenu].ptr;
				for (i = Menu->selected; mousewheel; mousewheel -= (mousewheel > 0 ? 1 : -1)) {
					do {
						i += (mousewheel > 0 ? -1 : 1);
						if (i == Menu->selected)
							break;
						if (i < 0)
							i = Menu->n_entries - 1;
						else if (i >= Menu->n_entries)
							i = 0;
					} while (Menu->entries[i].class_ == &MenuSeparatorClass || MENUITEM(&Menu->entries[i])->disabled);
				}
				Menu->selected = i;
				Redraw = TRUE;
			}
			SelectedItem = -1;
			if (SelectedMenu >= 0) {
				Menu = MenuBar[SelectedMenu].ptr;
				if (mousex != oldmousex || mousey != oldmousey || Click == 1) {
					SelectedItem = ClickWidgets(Menu->entries, Menu->n_entries, mousex, mousey, NULL);
					if (SelectedItem >= 0 && Menu->entries[SelectedItem].class_ != &MenuSeparatorClass && !MENUITEM(&Menu->entries[SelectedItem])->disabled) {
						if (Menu->selected != SelectedItem) {
							Redraw = TRUE;
							Menu->selected = SelectedItem;
						}
					}
				} else if (joystick1 & JOYPAD_A) {
					SelectedItem = Menu->selected;
					Redraw = TRUE;
				}
			}
			if ((Click == 1 || (joystick1 & JOYPAD_A)) && SelectedItem >= 0) {
				i = DoMenuCommand(SelectedMenu, SelectedItem);
				if (i != 0) {
					if (i == -2) {
						if (playstate == EX_STILLPLAYING || playstate == EX_AUTOMAP) {
							playstate = EX_STILLPLAYING;
							SetAPalette(rGamePal);			/* Reset the game palette */
							RedrawStatusBar();		/* Redraw the lower area */
							RenderView();
							if (ShapePtr) {
								DrawShape((SCREENWIDTH-224)/2, (MacViewHeight-56)/2, ShapePtr);
								BlitScreen();
							}
							goto Draw;
						} else
							i = EX_RESTART;
					} else if (i <= -3) {
						OpenDialog = -i - 2;
						switch (OpenDialog) {
							case 1: InitVideoDialog(); break;
							case 2: InitKeyboardDialog(); break;
							default: break;
						}
						SelectedMenu = -1;
						goto Draw;
					}
					if (i >= 0)
						RetVal = i;
					break;
				}
				if (SelectedMenu == 0)
					SelectedMenu = -1;
				Redraw = TRUE;
			} else if (mousex != oldmousex || mousey != oldmousey || Click == 1) {
				i = ClickWidgets(MenuBar, 2, mousex, mousey, NULL);
				if (Click == 1 && i == SelectedMenu && i >= 0) {
					SelectedMenu = -1;
					Redraw = TRUE;
				} else if (i != SelectedMenu && ((i >= 0 && SelectedMenu >= 0) || Click == 1)) {
					SelectedMenu = i;
					Redraw = TRUE;
					if (SelectedMenu >= 0) {
						Menu = MenuBar[SelectedMenu].ptr;
						Menu->selected = 0;
					}
				}
			}
		}
		if (Redraw) {
		Draw:
			BlitScreen();
			StartUIOverlay();
			switch (OpenDialog) {
				case 0: DrawMainMenu(SelectedMenu); break;
				case 1: DrawVideoDialog(); break;
				case 2: DrawKeyboardDialog(); break;
					default: break;
			}
			EndUIOverlay();
		}
	}
	if (ShapePtr)
		FreeSomeMem(ShapePtr);
	mousex=0;		/* Discard unpausing events */
	mousey=0;
	mouseturn=0;
	mousebuttons=0;
	joystick1=0;
	if (playstate == EX_STILLPLAYING)
		GrabMouse();
	ResumeSoundMusicSystem();
	return RetVal;
}
