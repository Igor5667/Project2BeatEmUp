#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<string.h>

extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}

//=================================
//    INICJALIZACJA STA£YCH
//=================================

// ekran
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define HORIZON_Y 200
#define CAMERA_MARGIN 100
#define FPS_REFRESH 0.5
// player
#define PLAYER_START_X_POS 45
#define PLAYER_SPEED 350.0 // pixele na sekundê
#define DASH_SPEED 700.0
#define PLAYER_WIDTH 128
#define PLAYER_HEIGHT 128
#define HURTBOX_WIDTH 50
#define HURTBOX_HEIGHT 98
#define HURTBOX_OFF_X 39
#define HURTBOX_OFF_Y 39
// predkoœci akcji
#define ANIM_SPEED_WALK 0.1
#define ANIM_SPEED_IDLE 0.35
#define TIME_ATTACK_LIGHT 0.3
#define TIME_ATTACK_HEAVY 0.8
#define TIME_JUMP 1.0
#define TIME_DASH 0.3
// directions
#define LEFT 1
#define RIGHT 0
// actions
#define ACTION_IDLE 0
#define ACTION_LIGHT 1
#define ACTION_HEAVY 2
#define ACTION_JUMP 3
#define ACTION_WALK 4
#define ACTION_DASH 5
// buffer
#define INPUT_BUFFER_SIZE 5
#define MAX_SEQ_LEN 10
#define MAX_SEQUENCES 5
// info
#define INFO_MARGIN 4
#define INFO_BOX_HEIGHT 54
#define INFO_TEXT_BUFFER_SIZE 128
#define CHAR_WIDTH 8
#define INFO_LINE_1_Y 10
#define INFO_LINE_2_Y 26
#define INFO_LINE_3_Y 42

enum GameInput {
	INPUT_NONE = 0,
	INPUT_UP,
	INPUT_DOWN,
	INPUT_LEFT,
	INPUT_RIGHT,
	INPUT_ATTACK_LIGHT,
	INPUT_ATTACK_HEAVY,
	INPUT_DEV_MODE
};

//=================================
//       STRUKTURY
//=================================

struct AttackData {
	int width;
	int height;
	int yOffset;
	int damage;
	bool active;
};

struct Sequence {
	const char* name;
	int sequence[MAX_SEQ_LEN];
	int len;
	int maxTimeGap;
};

struct InputEvenet {
	int input;
	Uint32 time;
};

struct PlayerTextures {
	SDL_Texture* idle;
	int idleFrames;
	SDL_Texture* attLight;
	int attLightFrames;
	SDL_Texture* attHeavy;
	int attHeavyFrames;
	SDL_Texture* jump;
	int jumpFrames;
	SDL_Texture* dash;
	int dashFrames;
	SDL_Texture* walk;
	int walkFrames;
};

struct Player {
	int x;
	int y;
	int w;
	int h;
	double speed;
	int direction;
	int currentAction;   // Przechowuje stan (IDLE, LIGHT, HEAVY)
	double actionTimer;
	bool hasHit;

	PlayerTextures textures;
	int currentFrame;
	double animTimer;
};

struct Enemy {
	int x, y;
	int h, w;
	double hitTimer;
};

struct Colors {
	int black;
	int green;
	int red;
	int blue;
	int lightBlue;
	int lightGreen;
};

struct Camera {
	int x;
	int y;
	int w;
	int h;
};

struct Time {
	double worldTime;
	double delta;
	int t1;
	int t2;
	int frames;
	double fpsTimer;
	double fps;
};

struct Buffer {
	InputEvenet inputs[INPUT_BUFFER_SIZE];
	int headIndex;
	int previousHeadIndex;
	bool showDebug;
};

struct Score {
	int points;
	double comboMultipler;
	double comboTimer;
};

// g³ówna struktura stanu gry
struct GameState {
	Player player;
	Enemy enemy;
	Colors colors;
	Camera camera;
	Time time;
	Buffer buffer;
	Score score;
	int quit;

	Sequence definedSeqences[MAX_SEQUENCES];
	int sequencesCount;
};

// struktura obs³uguj¹ca bibliotekê SDL
struct SDLContext {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Surface* screen;
	SDL_Texture* scrtex;
	SDL_Surface* charset;

	SDL_Texture* enemyIdle;
	int enemyWidth;
	int enemyHeight;

	SDL_Texture* background;
	int bgWidth;
	int bgHeight;
};

// tablice nazw wprowadzonych akcji (do opcji debug)
const char* actionNames[] = {
	"IDLE",     // 0
	"LIGHT",    // 1
	"HEAVY"     // 2
	"JUMP"      // 3
	"WALK"      // 4
	"DASH"      // 5
};
const char* inputNames[] = {
	"...",      // INPUT_NONE
	"UP",       // INPUT_UP
	"DOWN",     // INPUT_DOWN
	"LEFT",     // INPUT_LEFT
	"RIGHT",    // INPUT_RIGHT
	"L_ATK",    // INPUT_ATTACK_LIGHT
	"H_ATK",    // INPUT_ATTACK_HEAVY
	"DEV"       // INPUT_DEV_MODE
};

// dane do hitboxów ataków
const AttackData attacksData[] = {
	{ 0, 0, 0, 0, false },     // ACTION_IDLE
	{ 60, 50, 60, 10, true },  // ACTION_LIGHT
	{ 120, 60, 70, 20, true },  // ACTION_HEAVY
	{ 60, 90, 120, 15, true }   // ACTION_JUMP
};


//=================================
//       FUNKCJE POMOCNICZE
//=================================

SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path, int* outW, int* outH) {
	SDL_Surface* surface = SDL_LoadBMP(path);
	if (!surface) {
		printf("SDL_LoadBMP(%s) error: %s\n", path, SDL_GetError());
		return NULL;
	}

	if (outW) *outW = surface->w;
	if (outH) *outH = surface->h;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	if (!texture) {
		printf("SDL_CreateTextureFromSurface(%s) error: %s\n", path, SDL_GetError());
	}
	return texture;
}

SDL_Rect getAttackBox(Player* player) {
	SDL_Rect box = { 0, 0, 0, 0 };

	if (player->currentAction < 0 || player->currentAction > 3) return box;

	AttackData data = attacksData[player->currentAction];

	if (!data.active) return box;

	box.w = data.width;
	box.h = data.height;
	box.y = player->y - data.yOffset;

	// ustawianie kierunku ataku
	if (player->direction == RIGHT) {
		box.x = player->x + player->w - HURTBOX_OFF_X;
	}
	else {
		box.x = player->x - box.w + HURTBOX_OFF_X;
	}

	return box;
}

// funkcja sprawdza element n-ty od koñca
// 0 - zwraca ostatni, 1 - przedostatni
InputEvenet* getInputBack(Buffer* buffer, int stepsBack) {
	int index = (buffer->headIndex - 1 - stepsBack + INPUT_BUFFER_SIZE) % INPUT_BUFFER_SIZE;
	return &buffer->inputs[index];
}

//=================================
//       FUNCKCJE SETUP
//=================================

void initPlayer(Player* player) {
	player->w = PLAYER_WIDTH;
	player->h = PLAYER_HEIGHT;
	player->x = SCREEN_WIDTH / 2;
	player->y = SCREEN_HEIGHT / 2;
	player->speed = PLAYER_SPEED;
	player->direction = RIGHT;
	player->currentAction = ACTION_IDLE;
	player->actionTimer = 0.0;
	player->hasHit = false;
	player->currentFrame = 0;
	player->animTimer = 0.0;
}

void initEnemy(Enemy* enemy, int w, int h) {
	enemy->x = 500;
	enemy->y = 300;
	enemy->w = w; // Przyk³adowa szerokoœæ
	enemy->h = h; // Przyk³adowa wysokoœæ
}

void initCamera(Camera* camera) {
	camera->x = -100;
	camera->y = 0;
	camera->w = SCREEN_WIDTH;
	camera->h = SCREEN_HEIGHT;
}

void initTime(Time* time) {
	time->worldTime = 0;
	time->t1 = SDL_GetTicks();
	time->frames = 0;
	time->fpsTimer = 0;
	time->fps = 0;
}

void initColors(Colors* colors, const SDL_PixelFormat* format) {
	colors->black = SDL_MapRGB(format, 0x00, 0x00, 0x00);
	colors->green = SDL_MapRGB(format, 0x00, 0xFF, 0x00);
	colors->red = SDL_MapRGB(format, 0xFF, 0x00, 0x00);
	colors->blue = SDL_MapRGB(format, 0x11, 0x11, 0xCC);
	colors->lightBlue = SDL_MapRGB(format, 0x5D, 0xED, 0xF7);
	colors->lightGreen = SDL_MapRGB(format, 0x7E, 0xC8, 0x50);
}

void initBuffer(Buffer* buffer) {
	buffer->headIndex = 0;
	buffer->previousHeadIndex = 0;
	buffer->showDebug = false;

	for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
		buffer->inputs[i].input = INPUT_NONE;
		buffer->inputs[i].time = 0;
	}
}

void initSequences(GameState* state) {
	state->sequencesCount = 0;

	Sequence* s1 = &state->definedSeqences[state->sequencesCount++];
	s1->name = "Triple hit";
	s1->len = 3;
	s1->sequence[0] = INPUT_ATTACK_LIGHT;
	s1->sequence[1] = INPUT_ATTACK_LIGHT;
	s1->sequence[2] = INPUT_ATTACK_LIGHT;
	s1->maxTimeGap = 250;

	Sequence* s2 = &state->definedSeqences[state->sequencesCount++];
	s2->name = "Dash Right";
	s2->len = 2;
	s2->sequence[0] = INPUT_RIGHT;
	s2->sequence[1] = INPUT_RIGHT;
	s2->maxTimeGap = 200;

	Sequence* s3 = &state->definedSeqences[state->sequencesCount++];
	s3->name = "Dash Left";
	s3->len = 2;
	s3->sequence[0] = INPUT_LEFT;
	s3->sequence[1] = INPUT_LEFT;
	s3->maxTimeGap = 200;

	Sequence* s4 = &state->definedSeqences[state->sequencesCount++];
	s4->name = "Jump";
	s4->len = 4;
	s4->sequence[0] = INPUT_ATTACK_HEAVY;
	s4->sequence[1] = INPUT_ATTACK_LIGHT;
	s4->sequence[2] = INPUT_ATTACK_HEAVY;
	s4->sequence[3] = INPUT_ATTACK_LIGHT;
	s4->maxTimeGap = 200;
}

void initScore(Score* score) {
	score->points = 0;
	score->comboMultipler = 1;
	score->comboTimer = 0.0;
}

// inicjalizacja stanu gry oraz kolorów(dlatego przekazujemy surface ekranu)
void initGameState(GameState* state, const SDLContext* sdl) {
	initPlayer(&state->player);
	initEnemy(&state->enemy, sdl->enemyWidth, sdl->enemyHeight);
	initCamera(&state->camera);
	initTime(&state->time);
	initColors(&state->colors, sdl->screen->format);
	initBuffer(&state->buffer);
	initSequences(state);
	initScore(&state->score);
	state->quit = 0;
}

// funkcja inicjalizuj¹ca bibliotekê SDL
bool initSDL(SDLContext* sdl) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		return 1;
	}

	// tryb pe³noekranowy / fullscreen mode
//	int rc = SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP,
//	                                 &window, &renderer);
	int rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0,
		&sdl->window, &sdl->renderer);
	if (rc != 0) {
		SDL_Quit();
		printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
		return 1;
	};

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(sdl->renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(sdl->renderer, 0, 0, 0, 255);
	SDL_SetWindowTitle(sdl->window, "Szablon do zdania drugiego 2017");

	sdl->screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

	sdl->scrtex = SDL_CreateTexture(sdl->renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH, SCREEN_HEIGHT);

	SDL_SetTextureBlendMode(sdl->scrtex, SDL_BLENDMODE_BLEND);

	// wy³¹czenie widocznoœci kursora myszy
	SDL_ShowCursor(SDL_DISABLE);
}

// funkcja ³aduj¹ca zasoby (obrazy, czcionki, itp)
bool loadAssets(SDLContext* sdl, GameState* state) {
	// wczytanie czcionki cs8x8.bmp
	sdl->charset = SDL_LoadBMP("./textures/cs8x8.bmp");
	if (sdl->charset == NULL) {
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		return 1;
	};
	SDL_SetColorKey(sdl->charset, true, 0x000000);

	// wczytywanie tekstur
	sdl->enemyIdle = loadTexture(sdl->renderer, "./textures/enemy_idle.bmp", &sdl->enemyWidth, &sdl->enemyHeight);
	sdl->background = loadTexture(sdl->renderer, "./textures/background.bmp", &sdl->bgWidth, &sdl->bgHeight);

	int textureWidth, textureHeight;
	state->player.textures.idle = loadTexture(sdl->renderer, "./textures/aminations/player_idle.bmp", &textureWidth, &textureHeight);
	state->player.textures.idleFrames = textureWidth / PLAYER_WIDTH;

	state->player.textures.walk = loadTexture(sdl->renderer, "./textures/aminations/player_walk.bmp", &textureWidth, &textureHeight);
	state->player.textures.walkFrames = textureWidth / PLAYER_WIDTH;

	state->player.textures.attLight = loadTexture(sdl->renderer, "./textures/aminations/player_attack_light.bmp", &textureWidth, &textureHeight);
	state->player.textures.attLightFrames = textureWidth / PLAYER_WIDTH;

	state->player.textures.attHeavy = loadTexture(sdl->renderer, "./textures/aminations/player_attack_heavy.bmp", &textureWidth, &textureHeight);
	state->player.textures.attHeavyFrames = textureWidth / PLAYER_WIDTH;

	state->player.textures.jump = loadTexture(sdl->renderer, "./textures/aminations/player_jump.bmp", &textureWidth, &textureHeight);
	state->player.textures.jumpFrames = textureWidth / PLAYER_WIDTH;

	state->player.textures.dash = loadTexture(sdl->renderer, "./textures/aminations/player_dash.bmp", &textureWidth, &textureHeight);
	state->player.textures.dashFrames = textureWidth / PLAYER_WIDTH;

	if(!sdl->background || !state->player.textures.idle || !state->player.textures.walk || !state->player.textures.attLight 
	|| !state->player.textures.attHeavy || !state->player.textures.jump) {
		return false;
	}

	return true;
}

// funkcja czyszcz¹ca miejsce po SDL
void cleanup(SDLContext* sdl, GameState* state) {
	// Zwalnianie powierzchni (Surface)
	if (sdl->charset) SDL_FreeSurface(sdl->charset);
	if (sdl->screen)  SDL_FreeSurface(sdl->screen);

	// Zwalnianie tekstur, renderera i okna
	if (sdl->background)   SDL_DestroyTexture(sdl->background);
	if (sdl->scrtex)   SDL_DestroyTexture(sdl->scrtex);
	if (sdl->renderer) SDL_DestroyRenderer(sdl->renderer);
	if (sdl->window)   SDL_DestroyWindow(sdl->window);
	if (sdl->enemyIdle) SDL_DestroyTexture(sdl->enemyIdle);

	// Zwalnianie tekstur gracza
	if (state->player.textures.idle) SDL_DestroyTexture(state->player.textures.idle);
	if (state->player.textures.walk) SDL_DestroyTexture(state->player.textures.walk);
	if (state->player.textures.attLight) SDL_DestroyTexture(state->player.textures.attLight);
	if (state->player.textures.attHeavy) SDL_DestroyTexture(state->player.textures.attHeavy);
	if (state->player.textures.jump) SDL_DestroyTexture(state->player.textures.jump);

	// Wy³¹czenie SDL
	SDL_Quit();
}


//=================================
//       FUNCKCJE DRAW
//=================================

// narysowanie napisu txt na powierzchni screen, zaczynaj¹c od punktu (x, y)
// charset to bitmapa 128x128 zawieraj¹ca znaki
void DrawString(SDL_Surface *screen, int x, int y, const char *text,
                SDL_Surface *charset) {
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8;
	d.h = 8;
	while(*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 8;
		text++;
		};
	};

// narysowanie na ekranie screen powierzchni sprite w punkcie (x, y)
// (x, y) to punkt œrodka obrazka sprite na ekranie
void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x;
	dest.y = y - sprite->h;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
	};

// rysowanie pojedynczego pixela
void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	*(Uint32 *)p = color;
	};

// rysowanie linii o d³ugoœci l w pionie (gdy dx = 0, dy = 1) 
// b¹dŸ poziomie (gdy dx = 1, dy = 0)
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color) {
	for(int i = 0; i < l; i++) {
		DrawPixel(screen, x, y, color);
		x += dx;
		y += dy;
		};
	};

// rysowanie prostok¹ta o d³ugoœci boków l i k
// gdy fillColor = -1 to bez wnêtrza
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k,
                   Uint32 outlineColor, Uint32 fillColor) {
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);

	if (fillColor == -1) return;
	for(i = y + 1; i < y + k - 1; i++)
		DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
	};

// funkcja rysuj¹ca pod³ogê i t³o o okreœlonych kolorach
// na podstawie pozycji kamery
void drawScene(SDL_Renderer* renderer, SDL_Texture* background, Camera* camera) {
	SDL_Rect srcRect = {
		(int)camera->x,
		(int)camera->y,
		camera->w,
		camera->h
	};
	SDL_Rect dstRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	SDL_RenderCopy(renderer, background, &srcRect, &dstRect);


	//SDL_Rect rect = { camera->x, camera->y, camera->w, camera->h };
	//SDL_BlitSurface(background, &dest, screen, NULL);
}

void drawInfo(SDLContext* sdl, GameState* state) {
	char text[INFO_TEXT_BUFFER_SIZE];

	// pude³ko na info
	DrawRectangle(sdl->screen,
		INFO_MARGIN,
		INFO_MARGIN,
		SCREEN_WIDTH - (2 * INFO_MARGIN),
		INFO_BOX_HEIGHT,
		state->colors.red,
		state->colors.blue
	);

	// linia 1
	sprintf(text, "Czas trwania: %.1lf s  %.0lf fps", state->time.worldTime, state->time.fps);
	DrawString(sdl->screen,
		sdl->screen->w / 2 - (strlen(text) * CHAR_WIDTH) / 2,
		INFO_LINE_1_Y,
		text,
		sdl->charset
	);

	// linia 2
	sprintf(text, "Zrealizowano: obligatory-5/5 optional-0/8");
	DrawString(sdl->screen,
		sdl->screen->w / 2 - (strlen(text) * CHAR_WIDTH) / 2,
		INFO_LINE_2_Y,
		text,
		sdl->charset
	);

	sprintf(text, "Uzyskane punkty: %d   Mnoznik combo: %.1lf", state->score.points, state->score.comboMultipler);
	DrawString(sdl->screen,
		sdl->screen->w / 2 - (strlen(text) * CHAR_WIDTH) / 2,
		INFO_LINE_3_Y,
		text,
		sdl->charset
	);
}

SDL_Rect getPlayerHurtbox(Player* player) {
	SDL_Rect box;
	box.x = player->x + HURTBOX_OFF_X;
	box.y = player->y + HURTBOX_OFF_Y;

	box.w = HURTBOX_WIDTH;
	box.h = HURTBOX_HEIGHT;

	return box;
}

SDL_Texture* getCurrentPlayerTexture(Player* player) {
	switch (player->currentAction) {
	case ACTION_IDLE: return player->textures.idle;
	case ACTION_LIGHT: return player->textures.attLight;
	case ACTION_HEAVY: return player->textures.attHeavy;
	case ACTION_JUMP: return player->textures.jump;
	case ACTION_WALK: return player->textures.walk;
	case ACTION_DASH: return player->textures.dash;
	default: return player->textures.idle;
	}
}

void drawPlayer(SDL_Renderer* renderer, Player* player, Camera* camera) {
	SDL_Texture* textureToDraw = getCurrentPlayerTexture(player);

	SDL_Rect srcRect;
	srcRect.x = player->currentFrame * PLAYER_WIDTH;
	srcRect.y = 0;
	srcRect.w = PLAYER_WIDTH;
	srcRect.h = PLAYER_HEIGHT;

	SDL_Rect destRect;
	destRect.x = player->x - camera->x;
	destRect.y = player->y - camera->y - PLAYER_WIDTH;
	destRect.w = PLAYER_WIDTH;
	destRect.h = PLAYER_HEIGHT;

	SDL_RendererFlip flip = SDL_FLIP_NONE;
	if (player->direction == LEFT) {
		flip = SDL_FLIP_HORIZONTAL;
	}

	SDL_RenderCopyEx(renderer, textureToDraw, &srcRect, &destRect, 0, NULL, flip);
}

void drawDebugOverlay(SDLContext* sdl, GameState* state) {
	if (state->buffer.showDebug) {
		char text[512];
		char temp[64];

		sprintf(text, "DEBUG: Action: %s | Buffer: [ ", actionNames[state->player.currentAction]);
		for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
            sprintf(temp, "%s ", inputNames[state->buffer.inputs[i].input]);
            strcat(text, temp);
        }
        strcat(text, "]");

		DrawString(sdl->screen, 10, SCREEN_HEIGHT-15, text, sdl->charset);
	}
}

void drawEnemy(SDL_Renderer* renderer, Enemy* enemy, Camera* camera, SDL_Texture* enemyTex) {
	SDL_Rect destRect;
	destRect.x = enemy->x - camera->x;
	destRect.y = enemy->y - camera->y;
	destRect.w = enemy->w;
	destRect.h = enemy->h;

	// je¿eli jest uderzony to czerwony
	if (enemy->hitTimer > 0)  SDL_SetTextureColorMod(enemyTex, 255, 50, 50);

	SDL_RenderCopyEx(renderer, enemyTex, NULL, &destRect, 0.0, NULL, SDL_FLIP_HORIZONTAL);

	// powrot do normalnego kolorut
	SDL_SetTextureColorMod(enemyTex, 255, 255, 255);
}

void drawHitboxes(SDLContext* sdl, GameState* state) {
	if (!state->buffer.showDebug) return;

	Camera* cam = &state->camera;
	Colors* col = &state->colors;

	// rysowanie hurtboxa playera
	SDL_Rect playerBox = getPlayerHurtbox(&state->player);
	int px = playerBox.x - cam->x;
	int py = playerBox.y - state->player.h - cam->y;
	DrawRectangle(sdl->screen, px, py, playerBox.w, playerBox.h, col->green, -1);

	// Rysowanie hurtboxa enemy
	int ex = state->enemy.x - cam->x;
	int ey = state->enemy.y - cam->y;
	DrawRectangle(sdl->screen, ex, ey, state->enemy.w, state->enemy.h, col->green, -1);

	// Rysowanie hitboxa ataków
	SDL_Rect attackBox = getAttackBox(&state->player);
	if (attackBox.w > 0) {
		DrawRectangle(sdl->screen,
			attackBox.x - cam->x,
			attackBox.y - cam->y,
			attackBox.w,
			attackBox.h,
			col->red, -1);
	}
}

//=================================
//       FUNCKCJE LOGIC
//=================================

void startAction(Player* player, int action, double duration) {
	// Ustawiamy now¹ akcjê i czas jej trwania
	player->currentAction = action;
	player->actionTimer = duration;

	// KLUCZOWE: Resetujemy animacjê do pocz¹tku!
	player->currentFrame = 0;
	player->animTimer = 0.0;

	// Resetujemy flagê trafienia (wa¿ne dla ataków)
	player->hasHit = false;
}

bool checkRectCollision(SDL_Rect a, SDL_Rect b) {
	if (a.x + a.w < b.x) return false;
	if (a.x > b.x + b.w) return false;
	if (a.y + a.h < b.y) return false;
	if (a.y > b.y + b.h) return false;
	return true;
}

void handleAttacks(GameState* state) {
	if (state->enemy.hitTimer > 0) {
		state->enemy.hitTimer -= state->time.delta;
	}

	SDL_Rect attackBox = getAttackBox(&state->player);
	if (attackBox.w == 0 || state->player.hasHit) return;

	SDL_Rect enemyBox;
	enemyBox.x = state->enemy.x;
	enemyBox.y = state->enemy.y;
	enemyBox.w = state->enemy.w;
	enemyBox.h = state->enemy.h;

	if (checkRectCollision(attackBox, enemyBox)) {
		state->player.hasHit = true;
		state->enemy.hitTimer = 0.2;

		state->score.comboTimer = 2.0;
		state->score.points += attacksData[state->player.currentAction].damage * state->score.comboMultipler;
		state->score.comboMultipler += 0.4;

		if (state->buffer.showDebug) {
			printf("HIT! Action ID: %d\n", state->player.currentAction);
		}
	}
}

bool checkSequence(Buffer* buffer, Sequence* sequence) {
	for (int i = 0; i < sequence->len; i++) {
		int requiredInput = sequence->sequence[sequence->len - 1 - i];
		InputEvenet* event = getInputBack(buffer, i);

		// sprawdzanie czy odpowiedni input
		if (event->input != requiredInput) {
			return false;
		}

		// sprawdzanie czy za wolno
		if (i > 0) {
			InputEvenet* nextEvent = getInputBack(buffer, i - 1);
			Uint32 diff = nextEvent->time - event->time;
			if (diff > sequence->maxTimeGap) {
				return false;
			}
		}
	}

	// Jeœli pêtla przesz³a do koñca, to znaczy, ¿e wszystko pasuje!
	return true;
}

void resolveInputs(GameState* state) {
	// sprawdzanie combosów
	for (int i = 0; i < state->sequencesCount; i++) {
		if (checkSequence(&state->buffer, &state->definedSeqences[i])) {

			// Logujemy sukces w konsoli
			if (state->buffer.showDebug) {
				printf(">>> COMBO DETECTED: %s <<<\n", state->definedSeqences[i].name);
			}

			// co ma sie staæ w danym combo
			if (strcmp(state->definedSeqences[i].name, "Triple hit") == 0) {
				startAction(&state->player, ACTION_HEAVY, TIME_ATTACK_HEAVY);
			}
			else if (strcmp(state->definedSeqences[i].name, "Jump") == 0) {
				startAction(&state->player, ACTION_JUMP, TIME_JUMP);
			}
			else if (strcmp(state->definedSeqences[i].name, "Dash Right") == 0) {
				startAction(&state->player, ACTION_DASH, TIME_DASH);
				state->player.direction = RIGHT;
			}
			else if (strcmp(state->definedSeqences[i].name, "Dash Left") == 0) {
				startAction(&state->player, ACTION_DASH, TIME_DASH);
				state->player.direction = LEFT;
			}

			return;
		}
	}

	// sprawdzanie podjedyñczych klikniêæ tykji w idle
	if (state->player.currentAction == ACTION_IDLE) {
		InputEvenet* lastEvent = getInputBack(&state->buffer, 0);

		if (lastEvent->input == INPUT_ATTACK_LIGHT) {
			startAction(&state->player, ACTION_LIGHT, TIME_ATTACK_LIGHT);
		}

		if (lastEvent->input == INPUT_ATTACK_HEAVY) {
			startAction(&state->player, ACTION_HEAVY, TIME_ATTACK_HEAVY);
		}
	}
}

void updateScoreLogic(Score* score, double delta) {
	// ograniczenie combo
	if (score->comboMultipler > 5) {
		score->comboMultipler = 5;
	}

	if(score->comboTimer > 0) {
		score->comboTimer -= delta;
	} else {
		score->comboMultipler = 1.0;
	}
}

void updatePlayerAction(GameState* state) {
	if (state->buffer.headIndex != state->buffer.previousHeadIndex) {
		resolveInputs(state);
		state->buffer.previousHeadIndex = state->buffer.headIndex;
	}

	if (state->player.currentAction != ACTION_IDLE && state->player.currentAction != ACTION_WALK) {
		state->player.actionTimer -= state->time.delta;

		// Jeœli czas min¹³ koniec ataku
		if (state->player.actionTimer <= 0) {
			state->player.currentAction = ACTION_IDLE;
			state->player.actionTimer = 0;
		}
	}
}

void movePlayer(Player* player, double delta, int end) {
	// obs³uga dasha
	if (player->currentAction == ACTION_DASH) {
		if (player->direction == RIGHT) {
			player->x += (int)(DASH_SPEED * delta);
		}
		else {
			player->x -= (int)(DASH_SPEED * delta);
		}
	}// je¿eli dashuje to nie da siê sterowaæ
	else {
		// jak nie chodzi to powrót do idle
		if (player->currentAction == ACTION_WALK) {
			player->currentAction = ACTION_IDLE;
		}

		const Uint8* kaystate = SDL_GetKeyboardState(NULL);
		if (kaystate[SDL_SCANCODE_W]){
			player->y -= (int)(player->speed * delta);
			player->currentAction = ACTION_WALK;
		}
		if (kaystate[SDL_SCANCODE_S]) {
			player->y += (int)(player->speed * delta);
			player->currentAction = ACTION_WALK;
		}
		if (kaystate[SDL_SCANCODE_A]) {
			player->x -= (int)(player->speed * delta);
			player->currentAction = ACTION_WALK;
			player->direction = LEFT;
		}
		if (kaystate[SDL_SCANCODE_D]) { 
			player->x += (int)(player->speed * delta);
			player->currentAction = ACTION_WALK;
			player->direction = RIGHT;
		};
	}

	// Blokada wyjœcia z góry
	if (player->y < HORIZON_Y) {
		player->y = HORIZON_Y;
	} // Blokada wyjœcia z do³u
	if (player->y > SCREEN_HEIGHT) {
		player->y = SCREEN_HEIGHT;
	}

	// Blokada wyjœcia z lewej strony
	if (player->x < 0 - HURTBOX_OFF_X) {
		player->x = 0 - HURTBOX_OFF_X;
	}// Blokada wyjœcia z prawej strony
	if (player->x > end - player->w + HURTBOX_OFF_X) {
		player->x = end - player->w + HURTBOX_OFF_X;
	}
}

void updateTime(Time *time) {
	time->t2 = SDL_GetTicks();
	time->delta = (time->t2 - time->t1) * 0.001;
	time->t1 = time->t2;
	time->worldTime += time->delta;

	// obliczanie FPS
	time->fpsTimer += time->delta;
	if (time->fpsTimer > FPS_REFRESH) {
		time->fps = time->frames * 2;
		time->frames = 0;
		time->fpsTimer -= FPS_REFRESH;
	};
	time->frames++;
}

void updatePlayerAnimation(Player* player, double delta) {
	int maxFrames = 0;
	double frameDuration = 0.1;
	switch (player->currentAction) {
	case ACTION_IDLE:
		maxFrames = player->textures.idleFrames;
		frameDuration = ANIM_SPEED_IDLE;
		break;
	case ACTION_WALK:
		maxFrames = player->textures.walkFrames;
		frameDuration = ANIM_SPEED_WALK;
		break;
	case ACTION_LIGHT:
		maxFrames = player->textures.attLightFrames;
		if (maxFrames > 0) frameDuration = TIME_ATTACK_LIGHT / (double)maxFrames;
		break;
	case ACTION_HEAVY:
		maxFrames = player->textures.attHeavyFrames;
		if (maxFrames > 0) frameDuration = TIME_ATTACK_HEAVY / (double)maxFrames;
		break;
	case ACTION_JUMP:
		maxFrames = player->textures.jumpFrames;
		if (maxFrames > 0) frameDuration = TIME_JUMP / (double)maxFrames;
		break;
	case ACTION_DASH:
		maxFrames = player->textures.dashFrames;
		if (maxFrames > 0) frameDuration = TIME_DASH / (double)maxFrames;
		break;
	default:
		maxFrames = player->textures.idleFrames;
		frameDuration = 0.1;
		break;
	}
	if (maxFrames == 0) return;
	player->animTimer += delta;

	if (player->animTimer >= frameDuration) {
		player->animTimer -= frameDuration;
		player->currentFrame++;

		if (player->currentFrame >= maxFrames) {
			player->currentFrame = 0;
		}
	}
}

void updateCamera(GameState* state, int mapWidth) {
	// gdy gracz jest przy prawej krawêdzi ekranu to ruch kamer¹
	if ((state->player.x + state->player.w) > state->camera.x + SCREEN_WIDTH - CAMERA_MARGIN) {
		state->camera.x = (state->player.x + state->player.w) - (SCREEN_WIDTH - CAMERA_MARGIN);
	}// gdy gracz jest przy lewej krawêdzi ekranu to ruch kamer¹
	if (state->player.x < state->camera.x + CAMERA_MARGIN) {
		state->camera.x = state->player.x - CAMERA_MARGIN;
	}

	// blokada po lewej
	if (state->camera.x < 0) {
		state->camera.x = 0;
	}// blokada po prawej
	if (state->camera.x > mapWidth - SCREEN_WIDTH) {
		state->camera.x = mapWidth - SCREEN_WIDTH;
	}
}

void updateInputs(GameState* state) {
	
}

void pushInput(Buffer* buffer, int inputCode) {
	buffer->inputs[buffer->headIndex].input = inputCode;
	buffer->inputs[buffer->headIndex].time = SDL_GetTicks();
	buffer->headIndex = (buffer->headIndex + 1) % INPUT_BUFFER_SIZE;
}

// obs³ugiwanie prostych zdarzeñ typu escape
// albo zakoñczenie programu
void handleEvents(GameState* state, const SDLContext* sdl) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE) state->quit = 1;
				if (event.key.keysym.sym == SDLK_n) initGameState(state, sdl);

				if (event.key.repeat == 0) {
					int gameInput = INPUT_NONE;
					switch (event.key.keysym.sym) {
						case SDLK_w:	gameInput = INPUT_UP; break;
						case SDLK_s:	gameInput = INPUT_DOWN; break;
						case SDLK_a:	gameInput = INPUT_LEFT; break;
						case SDLK_d:    gameInput = INPUT_RIGHT; break;
						case SDLK_p:	gameInput = INPUT_ATTACK_LIGHT; break;
						case SDLK_o:	gameInput = INPUT_ATTACK_HEAVY; break;
						case SDLK_t:	state->buffer.showDebug = !state->buffer.showDebug; break;
					}
					if (gameInput != INPUT_NONE) {
						pushInput(&state->buffer, gameInput);
					}
				}
				break;
			case SDL_QUIT:
				state->quit = 1;
				break;
			};
	};
}

// aktualizacja stanu gry
void updateGame(GameState* state, SDLContext* sdl) {
	updatePlayerAction(state);
	updateTime(&state->time);
	movePlayer(&state->player, state->time.delta, sdl->bgWidth);
	updatePlayerAnimation(&state->player, state->time.delta);
	updateCamera(state, sdl->bgWidth);
	handleAttacks(state);
	updateScoreLogic(&state->score, state->time.delta);
}

void render(GameState* state, SDLContext* sdl) {
	// czyszczenie ekranu przed now¹ klatk¹
	SDL_FillRect(sdl->screen, NULL, SDL_MapRGBA(sdl->screen->format, 0, 0, 0, 0));

	drawScene(sdl->renderer, sdl->background, &state->camera);

	drawEnemy(sdl->renderer, &state->enemy, &state->camera, sdl->enemyIdle);

	drawPlayer(sdl->renderer, &state->player, &state->camera);

	drawHitboxes(sdl, state);
	drawInfo(sdl, state);
	drawDebugOverlay(sdl, state);

	// wysy³anie na ekran
	SDL_UpdateTexture(sdl->scrtex, NULL, sdl->screen->pixels, sdl->screen->pitch);
	SDL_RenderCopy(sdl->renderer, sdl->scrtex, NULL, NULL);
	SDL_RenderPresent(sdl->renderer);
}

//=================================
//         FUNKCJA MAIN
//=================================

#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char **argv) {
	SDLContext sdl = {};
	GameState state;

	// inicjalizacja biblioteki SDL
	if (!initSDL(&sdl)) {
		cleanup(&sdl, &state);
		return 1;
	}

	// pobieranie zasobów
	if (!loadAssets(&sdl, &state)) {
		cleanup(&sdl, &state);
		return 1;
	}

	// inicjalizacja stanu gry
	initGameState(&state, &sdl);

	// g³ówna pêtla gry
	while(!state.quit) {
		handleEvents(&state, &sdl);
		updateGame(&state, &sdl);
		render(&state, &sdl);
	};

	cleanup(&sdl, &state);
	return 0;
};
