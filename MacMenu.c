#include "Burger.h"
#include "WolfDef.h"
#include "SDLWolf.h"
#include <string.h>

typedef enum {
	WS_NORMAL,
	WS_SELECTED,
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
} scenario_t;

static LongWord MenuPosY = 0;
static LongWord MenuScrollY = 0;
static LongWord ScenarioCount = 0;
static SDL_Surface *MenuBG = NULL;
static scenario_t *Scenarios = NULL;
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

static void RenderWidgets(widget_t *Widgets, int N, int Selected, void *Data)
{
	widgetstate_t State;
	int Index;
	for (Index = 0; Index < N; Index++, Widgets++) {
		if (Widgets->class_->render) {
			if (Index == Selected)
				State = WS_SELECTED;
			else
				State = WS_NORMAL;
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
} menuitem_t;

typedef struct {
	LongWord *pos;
	LongWord *max;
} scrollbar_t;

static void ButtonRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	DrawFilledFrame(&Widget->rect);
	if (State != WS_NORMAL)
		SDL_RenderRects(SdlRenderer, (SDL_FRect[3]){
			FRectShrink(&Widget->rect, -2), FRectShrink(&Widget->rect, -3), FRectShrink(&Widget->rect, -4)}, 3);
	if (Widget->ptr) {
		FontSetColor(BLACK);
		FontSetClip(&Widget->rect);
		SetFontXY(Widget->rect.left + 8, Widget->rect.top + 3);
		DrawAString(Widget->ptr);
	}
}

static void TextEntryRender(widget_t *Widget, widgetstate_t State, void *Data)
{
	if (State != WS_NORMAL) {
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

	if (State == WS_SELECTED) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderFillRect(SdlRenderer,&FRect(&Widget->rect));
		DrawFilledFrame(&Menu->rect);
		DrawDropShadow(&Menu->rect);
		RenderWidgets(Menu->entries, Menu->n_entries, Menu->selected, Data);
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

	if (State == WS_SELECTED) {
		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderFillRect(SdlRenderer,&FRect(&Widget->rect));
	}
	if (Item->getvalue && Item->getvalue(Widget, Data)) {
		if (State == WS_SELECTED)
			SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
		else
			SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		for (int y = 0; y < 2; y++)
			SDL_RenderLines(SdlRenderer, (SDL_FPoint[3]){
				{X+3, Y+8+y}, {X+5, Y+10+y}, {X+11, Y+4+y},
			}, 3);
	}
	FontSetColor(State == WS_SELECTED ? WHITE2 : BLACK);
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
static widgetclass_t TextEntryClass = { TextEntryRender, NULL };
static widgetclass_t MenuBarItemClass = { MenuBarItemRender, NULL };
static widgetclass_t MenuItemClass = { MenuItemRender, NULL };
static widgetclass_t MenuSeparatorClass = { MenuSeparatorRender, NULL };
static widgetclass_t VScrollBarClass = { VScrollBarRender, VScrollBarClick };
static widgetclass_t EmptyClass = { NULL, NULL };

static SDL_EnumerationResult MakeScenarioList(void *UserData, const char *Dir, const char *Name)
{
	char *Buf;
	char TmpPath[strlen(Dir) + strlen(Name) + 1];
	RFILE *Rp;
	scenario_t *Scenario;

	Buf = SDL_strdup(Name);
	if (!Buf) BailOut("Out of memory");
	stpcpy(stpcpy(TmpPath, Dir), Name);
	Rp = res_open(TmpPath, 0);
	if (!Rp)
		return SDL_ENUM_CONTINUE;

	ScenarioCount++;
	Scenarios = SDL_realloc(Scenarios, ScenarioCount * sizeof(scenario_t));
	if (!Scenarios) BailOut("Out of memory");
	Scenario = &Scenarios[ScenarioCount-1];
	Scenario->pic = NULL;
	Scenario->name = Buf;
	Scenario->pic = LoadPict(Rp, 1);

	res_close(Rp);
	return SDL_ENUM_CONTINUE;
}

static void DrawScenarioName(const char *Name)
{
	char c;
	while (*Name == '_') Name++;
	for (; *Name && *Name != '.'; Name++) {
		c = *Name;
		if (c == '_')
			c = ' ';
		DrawAChar(c);
	}
}

static const Rect ScenarioListRect = {82, 14, 258, 339};
static const LongWord ScenariosItemHeight = (ScenarioListRect.bottom - ScenarioListRect.top)/16;
static widget_t ScenarioWidgets[2] = {
	{&VScrollBarClass,
	{ScenarioListRect.top, ScenarioListRect.right-1, ScenarioListRect.bottom, ScenarioListRect.right + 15},
	&(scrollbar_t){&MenuScrollY, &ScenarioCount}},
	{&ButtonClass, {288, 155, 308, 213}, "Cancel"}
};

static void DrawScenarioList(void)
{
	int i;
	Word Y;
	int Max;
	SDL_FRect FR;
	SDL_Surface *ScenarioPic = NULL;

	if (ScenarioCount)
		ScenarioPic = Scenarios[MenuPosY].pic;
	if (ScenarioPic)
		BlitSurface(ScenarioPic, &(SDL_Rect){231, 17, 96, 64});

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
		DrawScenarioName(Scenarios[i].name);
		Y += 16;
	}
	RenderWidgets(ScenarioWidgets, 2, -1, NULL);
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

Word ChooseScenario(void)
{
	int i;
	int Click = 0;
	int oldmousex, oldmousey;
	Boolean Redraw;
	Word RetVal = FALSE;

	if (ScenarioPath) SDL_free(ScenarioPath);
	ScenarioPath = NULL;
	EnumerateLevels(MakeScenarioList, NULL);
	MenuPosY = ScenarioIndex >= ScenarioCount ? ScenarioCount - 1 : ScenarioIndex;
	MenuScrollY = MenuPosY >= ScenariosItemHeight ? MenuPosY - ScenariosItemHeight + 1 : MenuPosY;
	ResizeGameWindow(369, 331);
	ClearTheScreen(WHITE);
	BlitScreen();
	MenuBG = LoadPict(MainResources, 129);
	if (MenuBG)
		BlitSurface(MenuBG, NULL);
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
			if (i == MenuPosY)
				ScenarioPath = Scenarios[i].name;
			else
				SDL_free(Scenarios[i].name);
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
		if (ScenarioPath) {
			SDL_free(ScenarioPath);
			ScenarioPath = NULL;
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
	if (ScenarioPath) {
		ClearTheScreen(BLACK);
		BlitScreen();
		RetVal = MountMapFile(ScenarioPath);
		LoadMapSetData();
		SDL_free(ScenarioPath);
		ScenarioPath = NULL;
	}
	return RetVal;
}

static Boolean GetSoundEnabled() { return (SystemState & SfxActive) != 0; }
static Boolean GetMusicEnabled() { return (SystemState & MusicActive) != 0; }
static Boolean GetMouseEnabled() { return MouseEnabled; }
static Boolean GetSlowDown() { return SlowDown; }
static Boolean GetPaused() { return TRUE; }

static widget_t MenuBar[2] = {
	{&MenuBarItemClass, {1, 12, 19, 53},&(menu_t) {"File", (widget_t[7]){
			{&MenuItemClass, {20, 13, 36, 144}, &(menuitem_t){"New Game..."}},
			{&MenuItemClass, {36, 13, 52, 144}, &(menuitem_t){"Open"}},
			{&MenuSeparatorClass, {52, 13, 68, 144}},
			{&MenuItemClass, {68, 13, 84, 144}, &(menuitem_t){"Save"}},
			{&MenuItemClass, {84, 13, 100, 144}, &(menuitem_t){"Save As..."}},
			{&MenuSeparatorClass, {100, 13, 116, 144}},
			{&MenuItemClass, {116, 13, 132, 144}, &(menuitem_t){"Quit"}},
		}, 7, {19, 12, 133, 145}, -1}},
	{&MenuBarItemClass, {1, 53, 19, 120},&(menu_t) {"Options", (widget_t[7]){
			{&MenuItemClass, {20, 54, 36, 233}, &(menuitem_t){"Sound", GetSoundEnabled}},
			{&MenuItemClass, {36, 54, 52, 233}, &(menuitem_t){"Music", GetMusicEnabled}},
			{&MenuItemClass, {52, 54, 68, 233}, &(menuitem_t){"Set Screen size..."}},
			{&MenuItemClass, {68, 54, 84, 233}, &(menuitem_t){"Speed Governor", GetSlowDown}},
			{&MenuItemClass, {84, 54, 100, 233}, &(menuitem_t){"Mouse Control", GetMouseEnabled}},
			{&MenuItemClass, {100, 54, 116, 233}, &(menuitem_t){"Pause", GetPaused}},
			{&MenuItemClass, {116, 54, 132, 233}, &(menuitem_t){"Configure Keyboard"}},
	}, 7, {19, 53, 133, 234}, -1}},
};

void DrawMainMenu(int SelectedMenu)
{
	SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
	SDL_RenderFillRect(SdlRenderer,&(SDL_FRect){0, 0, SCREENWIDTH, 19});
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderLine(SdlRenderer, 0, 19, SCREENWIDTH, 19);
	RenderWidgets(MenuBar, 2, SelectedMenu, NULL);
}

int DoMenuCommand(int Menu, int Item)
{
	switch (Menu) {
	case 0:				/* File menu */
		switch (Item) {
		case 0:
			if (ChooseGameDiff()) {		/* Choose level of difficulty */
				SaveFileName = 0;		/* Zap the save game name */
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
			if (SaveFileName) {			/* Save the file automatically? */
				SaveGame();				/* Save it */
				break;
			}
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
			SystemState^=SfxActive;				/* Sound on/off flags */
			if (!(SystemState&SfxActive)) {
				PlaySound(0);			/* Turn off all existing sounds */
			}
			break;
		case 1:
			SystemState^=MusicActive;			/* Music on/off flags */
			if (SystemState&MusicActive) {
				PlaySong(KilledSong);		/* Restart the music */
			} else {
				PlaySong(0);	/* Shut down the music */
			}
			break;
		case 2:
			return -3;
		case 3:
			SlowDown^=1;		/* Toggle the slow down flag */
			break;
		case 4:
			MouseEnabled = (!MouseEnabled);	/* Toggle the cursor */
			break;
		case 5:			/* Unpause */
			return -1;
		case 6:			/* Keyboard control window */
			return -4;
		}
		SavePrefs();
		break;
	}
	return 0;
}

static const Rect VideoDialogRect = {0, 0, 166, 318};
static const Rect ScreenButtonRect = {64, 50, 84, 138};

static widget_t ScreenSizeButtons[4] = {
	{&ButtonClass, {}, "320 x 200"},
	{&ButtonClass, {}, "512 x 384"},
	{&ButtonClass, {}, "640 x 400"},
	{&ButtonClass, {}, "640 x 480"},
};

static void InitVideoDialog(void)
{
	int X, Y;

	X = (SCREENWIDTH-VideoDialogRect.right)/2;
	Y = (SCREENHEIGHT-VideoDialogRect.bottom)/2;
	MenuScrollY = MenuPosY = GameViewSize;

	ScreenSizeButtons[0].rect = RectOff(&ScreenButtonRect, X,	 Y);
	ScreenSizeButtons[1].rect = RectOff(&ScreenButtonRect, X+124, Y);
	ScreenSizeButtons[2].rect = RectOff(&ScreenButtonRect, X,	 Y+36);
	ScreenSizeButtons[3].rect = RectOff(&ScreenButtonRect, X+124, Y+36);
}

static int RunVideoDialog(int Click, Boolean Moved)
{
	int i = -1;
	int Selected = -1;

	if (Click == 3 || (joystick1 & JOYPAD_B))
		return -1;
	if (joystick1 & JOYPAD_UP)
		MenuScrollY = MenuPosY & 1;
	if (joystick1 & JOYPAD_DN)
		MenuScrollY = MenuPosY | 2;
	if (joystick1 & JOYPAD_LFT)
		MenuScrollY = MenuPosY & 2;
	if (joystick1 & JOYPAD_RGT)
		MenuScrollY = MenuPosY | 1;
	if (Moved || Click == 1) {
		i = ClickWidgets(ScreenSizeButtons, 4, mousex, mousey, NULL);
		if (i >= 0) {
			MenuScrollY = i;
			if (Click == 1)
				Selected = i;
		}
	}
	if (joystick1 & JOYPAD_A)
		Selected = MenuScrollY;
	if (Selected >= 0) {
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
			}
			SavePrefs();
		}
		return -1;
	}
	if (MenuScrollY != MenuPosY) {
		MenuPosY = MenuScrollY;
		return 1;
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
	SetFontXY(X+51, Y+16);
	DrawAString("What screen size shall you conquer\nthe castle with?");
	RenderWidgets(ScreenSizeButtons, 4, MenuPosY, NULL);
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
		if (joystick1 & JOYPAD_UP)
			MenuScrollY = 0;
		if ((joystick1 & JOYPAD_LFT) && MenuPosY == 13)
			MenuScrollY = 12;
		if ((joystick1 & JOYPAD_RGT) && MenuPosY == 12)
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
				MenuScrollY = i+12;
				if (Click == 1) {
					MenuPosY = i+12;
					return -1;
				}
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
	RenderWidgets(KeyButtons, 12, MenuPosY, NULL);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SDL_RenderRect(SdlRenderer,&FRect(&RectOff(&KeysFrameRect, X, Y)));
	RenderWidgets(KeyOkCancelButtons, 2, MenuPosY-12, NULL);
}

static int GetAKey(void)
{
	SDL_Event event;

	joystick1 = 0;
	mousewheel = 0;

	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (ProcessGlobalEvent(&event))
			continue;
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

	if(playstate == EX_STILLPLAYING || playstate == EX_AUTOMAP)
		PauseSoundMusicSystem();
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
					} while (Menu->entries[i].class_ == &MenuSeparatorClass);
				}
				Menu->selected = i;
				Redraw = TRUE;
			}
			SelectedItem = -1;
			if (SelectedMenu >= 0) {
				Menu = MenuBar[SelectedMenu].ptr;
				if (mousex != oldmousex || mousey != oldmousey || Click == 1) {
					SelectedItem = ClickWidgets(Menu->entries, Menu->n_entries, mousex, mousey, NULL);
					if (SelectedItem >= 0 && Menu->entries[SelectedItem].class_ != &MenuSeparatorClass) {
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
