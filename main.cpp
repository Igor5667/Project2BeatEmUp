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

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PLAYER_START_X_POS 45
#define PLAYER_SPEED 500.0 // pixele na sekundê
#define FPS_REFRESH 0.5
#define HORIZON_Y 200
#define CAMERA_MARGIN 100
#define LEFT 1
#define RIGHT 0
#define ACTION_IDLE 0
#define ACTION_LIGHT 1
#define ACTION_HEAVY 2
#define INPUT_BUFFER_SIZE 5
#define MAX_SEQ_LEN 10
#define MAX_SEQUENCES 5

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

// tablice nazw wprowadzonych akcji (do opcji debug)
const char* actionNames[] = {
	"IDLE",     // 0
	"LIGHT",    // 1
	"HEAVY"     // 2
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


//=================================
//       STRUKTURY
//=================================

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

struct Player {
	int x;
	int y;
	int w;
	int h;
	double speed;
	int direction;
	int currentAction;   // Przechowuje stan (IDLE, LIGHT, HEAVY)
	double actionTimer;
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

// g³ówna struktura stanu gry
struct GameState {
	Player player;
	Colors colors;
	Camera camera;
	Time time;
	Buffer buffer;
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

	SDL_Texture* playerIdle;
	SDL_Texture* playerAttLight;
	SDL_Texture* playerAttHeavy;



	SDL_Texture* background;
	int playerWidth;
	int playerHeight;
	int bgWidth;
	int bgHeight;
};

//=================================
//       FUNCKCJE SETUP
//=================================

void initPlayer(Player* player, int w, int h) {
	player->w = w;
	player->h = h;
	player->x = SCREEN_WIDTH / 2;
	player->y = SCREEN_HEIGHT / 2;
	player->speed = PLAYER_SPEED;
	player->direction = RIGHT;
	player->currentAction = ACTION_IDLE;
	player->actionTimer = 0.0;
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
}

// inicjalizacja stanu gry oraz kolorów(dlatego przekazujemy surface ekranu)
void initGameState(GameState* state, const SDLContext* sdl) {
	initPlayer(&state->player, sdl->playerWidth, sdl->playerHeight);
	initCamera(&state->camera);
	initTime(&state->time);
	initColors(&state->colors, sdl->screen->format);
	initBuffer(&state->buffer);
	initSequences(state);
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
bool loadAssets(SDLContext* sdl, Player* player) {
	// wczytanie czcionki cs8x8.bmp
	sdl->charset = SDL_LoadBMP("./cs8x8.bmp");
	if (sdl->charset == NULL) {
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		return 1;
	};
	SDL_SetColorKey(sdl->charset, true, 0x000000);

	// wczytywanie obrazu playera IDLE
	sdl->playerIdle = loadTexture(sdl->renderer, "./player_idle.bmp",
		&sdl->playerWidth, &sdl->playerHeight);
	if (!sdl->playerIdle) {
		return false;
	}

	// wczytywanie obrazu playera LIGHT ATTACK
	sdl->playerAttLight = loadTexture(sdl->renderer, "./player_att_light.bmp",
		&sdl->playerWidth, &sdl->playerHeight);
	if (!sdl->playerAttLight) {
		return false;
	}

	// wczytywanie obrazu playera HEAVY ATTACK
	sdl->playerAttHeavy = loadTexture(sdl->renderer, "./player_att_heavy.bmp",
		&sdl->playerWidth, &sdl->playerHeight);
	if (!sdl->playerAttHeavy) {
		return false;
	}

	// Background - tekstura GPU
	sdl->background = loadTexture(sdl->renderer, "./background.bmp",
		&sdl->bgWidth, &sdl->bgHeight);
	if (!sdl->background) {
		return false;
	}

	return true;
}

// funkcja czyszcz¹ca miejsce po SDL
void cleanup(SDLContext* sdl) {
	// Zwalnianie powierzchni (Surface)
	if (sdl->charset) SDL_FreeSurface(sdl->charset);
	if (sdl->screen)  SDL_FreeSurface(sdl->screen);

	// Zwalnianie tekstur, renderera i okna
	if (sdl->playerIdle)   SDL_DestroyTexture(sdl->playerIdle);
	if (sdl->playerAttLight)   SDL_DestroyTexture(sdl->playerAttLight);
	if (sdl->playerAttHeavy)   SDL_DestroyTexture(sdl->playerAttHeavy);
	if (sdl->background)   SDL_DestroyTexture(sdl->background);
	if (sdl->scrtex)   SDL_DestroyTexture(sdl->scrtex);
	if (sdl->renderer) SDL_DestroyRenderer(sdl->renderer);
	if (sdl->window)   SDL_DestroyWindow(sdl->window);

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
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k,
                   Uint32 outlineColor, Uint32 fillColor) {
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
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
	char text[128];
	DrawRectangle(sdl->screen, 4, 4, SCREEN_WIDTH - 8, 36, state->colors.red, state->colors.blue);
	sprintf(text, "Czas trwania: %.1lf s  %.0lf fps", state->time.worldTime, state->time.fps);
	DrawString(sdl->screen, sdl->screen->w / 2 - strlen(text) * 8 / 2, 10, text, sdl->charset);
	sprintf(text, "Zrealizowano: obligatory-5/5 optional-0/8");
	DrawString(sdl->screen, sdl->screen->w / 2 - strlen(text) * 8 / 2, 26, text, sdl->charset);
}

void drawPlayer(SDL_Renderer* renderer, Player* player, Camera* camera, SDL_Texture* playerTex) {
	SDL_Rect destRect;
	destRect.x = (int)(player->x - camera->x);
	destRect.y = (int)(player->y - camera->y) - player->h;
	destRect.w = player->w;
	destRect.h = player->h;

	SDL_RendererFlip flip = (player->direction == LEFT) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(renderer, playerTex, NULL, &destRect, 0.0, NULL, flip);
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

//=================================
//       FUNCKCJE LOGIC
//=================================

// funkcja sprawdza element n-ty od koñca
// 0 - zwraca ostatni, 1 - przedostatni
InputEvenet* getInputBack(Buffer* buffer, int stepsBack) {
	int index = (buffer->headIndex - 1 - stepsBack + INPUT_BUFFER_SIZE) % INPUT_BUFFER_SIZE;
	return &buffer->inputs[index];
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
				state->player.currentAction = ACTION_HEAVY;
				state->player.actionTimer = 0.5;
			}
			else if (strcmp(state->definedSeqences[i].name, "Dash Right") == 0) {
				state->player.x += 100;
			}

			return;
		}
	}

	// sprawdzanie podjedyñczych klikniêæ
	InputEvenet* lastEvent = getInputBack(&state->buffer, 0);
	if (lastEvent->input == INPUT_ATTACK_LIGHT) {
		if (state->player.currentAction != ACTION_HEAVY) {
			state->player.currentAction = ACTION_LIGHT;
			state->player.actionTimer = 0.2;
		}
	}
}

void updatePlayerAction(GameState* state) {
	if (state->buffer.headIndex != state->buffer.previousHeadIndex) {
		resolveInputs(state);
		state->buffer.previousHeadIndex = state->buffer.headIndex;
	}

	if (state->player.currentAction != ACTION_IDLE) {
		state->player.actionTimer -= state->time.delta;

		// Jeœli czas min¹³ koniec ataku
		if (state->player.actionTimer <= 0) {
			state->player.currentAction = ACTION_IDLE;
			state->player.actionTimer = 0;
		}
	}
}

void movePlayer(Player* player, double delta, int end) {
	const Uint8* kaystate = SDL_GetKeyboardState(NULL);
	if (kaystate[SDL_SCANCODE_W]) player->y -= (int)(player->speed * delta);
	if (kaystate[SDL_SCANCODE_S]) player->y += (int)(player->speed * delta);
	if (kaystate[SDL_SCANCODE_A]) {
		player->x -= (int)(player->speed * delta);
		player->direction = LEFT;
	}
	if (kaystate[SDL_SCANCODE_D]) { 
		player->x += (int)(player->speed * delta);
		player->direction = RIGHT;
	};

	if (player->y < HORIZON_Y) {
		player->y = HORIZON_Y;
	}
	if (player->y > SCREEN_HEIGHT) {
		player->y = SCREEN_HEIGHT;
	}

	// Blokada wyjœcia z lewej strony
	if (player->x < 0) {
		player->x = 0;
	}
	// Blokada wyjœcia z prawej strony
	if (player->x > end - player->w) {
		player->x = end - player->w;
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

SDL_Texture* getCurrentPlayerTexture(SDLContext* sdl, int currAction) {
	switch (currAction) {
		case ACTION_IDLE:
			return sdl->playerIdle;
		case ACTION_LIGHT:
			return sdl->playerAttLight;
		case ACTION_HEAVY:
			return sdl->playerAttHeavy;
		default:
			return sdl->playerIdle;
	}
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
	updateCamera(state, sdl->bgWidth);
}

void render(GameState* state, SDLContext* sdl) {
	// czyszczenie ekranu przed now¹ klatk¹
	SDL_FillRect(sdl->screen, NULL, SDL_MapRGBA(sdl->screen->format, 0, 0, 0, 0));

	drawScene(sdl->renderer, sdl->background, &state->camera);

	SDL_Texture* currentPlayerTexture = getCurrentPlayerTexture(sdl, state->player.currentAction);
	drawPlayer(sdl->renderer, &state->player, &state->camera, currentPlayerTexture);

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
		cleanup(&sdl);
		return 1;
	}

	// pobieranie zasobów
	if (!loadAssets(&sdl, &state.player)) {
		cleanup(&sdl);
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

	cleanup(&sdl);
	return 0;
};
