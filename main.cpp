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

struct Player {
	int x;
	int y;
	int w;
	int h;
	double speed;
	int direction;
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

struct GameTime {
	double worldTime;
	double delta;
	int t1;
	int t2;
	int frames;
	double fpsTimer;
	double fps;
};

// g³ówna struktura stanu gry
struct GameState {
	Player player;
	Colors colors;
	Camera camera;
	GameTime time;
	int quit;
};

// struktura obs³uguj¹ca bibliotekê SDL
struct SDLContext {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Surface* screen;
	SDL_Texture* scrtex;
	SDL_Surface* charset;
	SDL_Texture* player;
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
}

void initCamera(Camera* camera) {
	camera->x = -100;
	camera->y = 0;
	camera->w = SCREEN_WIDTH;
	camera->h = SCREEN_HEIGHT;
}

void initTime(GameTime* time) {
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

// inicjalizacja stanu gry oraz kolorów(dlatego przekazujemy surface ekranu)
void initGameState(GameState* state, const SDLContext* sdl) {
	initPlayer(&state->player, sdl->playerWidth, sdl->playerHeight);
	initCamera(&state->camera);
	initTime(&state->time);
	initColors(&state->colors, sdl->screen->format);
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

	// wczytywanie obrazu playera
	sdl->player = loadTexture(sdl->renderer, "./player.bmp",
		&sdl->playerWidth, &sdl->playerHeight);
	if (!sdl->player) {
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
	if (sdl->player)   SDL_DestroyTexture(sdl->player);
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

//void drawPlayer(SDL_Renderer* renderer, const Player* player, const Camera* camera, SDL_Texture* playerTex) {
//	SDL_Rect dstRect = {
//		(int)(player->x - camera->x),
//		(int)(player->y - player->h - camera->y),
//		player->w,
//		player->h
//	};
//	SDL_RenderCopy(renderer, playerTex, NULL, &dstRect);
//}


//=================================
//       FUNCKCJE LOGIC
//=================================

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

void updateGameTime(GameTime *time) {
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

// obs³ugiwanie prostych zdarzeñ typu escape
// albo zakoñczenie programu
void handleEvents(GameState* state, const SDLContext* sdl) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE) state->quit = 1;
			if (event.key.keysym.sym == SDLK_n) initGameState(state, sdl);
			break;
		case SDL_QUIT:
			state->quit = 1;
			break;
		};
	};
}

// aktualizacja stanu gry
void updateGame(GameState* state, SDLContext* sdl) {
	printf("Player position: x=%d y=%d\n", state->player.x, state->player.y);
	updateGameTime(&(state->time));
	movePlayer(&(state->player), state->time.delta, sdl->bgWidth);
	updateCamera(state, sdl->bgWidth);
}

void render(GameState* state, SDLContext* sdl) {
	// rysowanie t³a i pod³ogi
	drawScene(sdl->renderer, sdl->background, &state->camera);

	// rysowanie gracza
	SDL_Rect destRect;
	destRect.x = (int)(state->player.x - state->camera.x);
	destRect.y = (int)(state->player.y - state->camera.y) - state->player.h;
	destRect.w = state->player.w;
	destRect.h = state->player.h;

	SDL_RendererFlip flip = (state->player.direction == LEFT) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(sdl->renderer, sdl->player, NULL, &destRect, 0.0, NULL, flip);

	// 
	SDL_FillRect(sdl->screen, NULL, 0x00000000);

	// rysowanie info na górze
	drawInfo(sdl, state);

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
	GameState state;
	SDLContext sdl = {};

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
