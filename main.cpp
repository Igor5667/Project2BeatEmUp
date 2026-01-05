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

#define SCREEN_WIDTH	1280
#define SCREEN_HEIGHT	720
#define PLAYER_START_X_POS     45
#define ETI_SPEED_VAL   1.0
#define MOVE_SPEED    500.0 // pixele na sekundê
#define FPS_REFRESH     0.5

//=================================
//       STRUKTURY
//=================================

// g³ówna struktura stanu gry
struct GameState {
	// Pozycja gracza (ETI)
	int etiPositionX; // Zmienilem na double dla plynnosci, w main bylo int, ale przy refactoringu warto trzymac pozycje jako double
	int etiPositionY;

	// Fizyka i czas
	double etiSpeed;
	double moveSpeed; // To bylo jako stala lokalna, przenosimy do stanu
	double distance;
	double worldTime;
	double delta;
	int t1;
	int t2;

	// Licznik FPS
	int frames;
	double fpsTimer;
	double fps;

	// Flaga wyjœcia
	int quit;

	// Kolory (zmapowane wartoœci pikseli)
	int czarny;
	int zielony;
	int czerwony;
	int niebieski;
};

// struktura obs³uguj¹ca bibliotekê SDL
struct SDLContext {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Surface* screen;
	SDL_Texture* scrtex;
	SDL_Surface* charset;
	SDL_Surface* eti;
};


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
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
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

//=================================
//       FUNCKCJE LOGIC
//=================================

// obs³ugiwanie prostych zdarzeñ typu escape
// albo zakoñczenie programu
void handleEvents(GameState* state) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE) state->quit = 1;
			break;
		case SDL_QUIT:
			state->quit = 1;
			break;
		};
	};
}

// aktualizacja stanu gry
void updateGame(GameState* state) {
	// obliczanie czasu globalnego
	state->t2 = SDL_GetTicks();
	state->delta = (state->t2 - state->t1) * 0.001;
	state->t1 = state->t2;
	state->worldTime += state->delta;

	// dodanie eti sterowany WSAD
	const Uint8* kaystate = SDL_GetKeyboardState(NULL);
	if (kaystate[SDL_SCANCODE_W]) state->etiPositionY -= (int)(state->moveSpeed * state->delta);
	if (kaystate[SDL_SCANCODE_S]) state->etiPositionY += (int)(state->moveSpeed * state->delta);
	if (kaystate[SDL_SCANCODE_A]) state->etiPositionX -= (int)(state->moveSpeed * state->delta);
	if (kaystate[SDL_SCANCODE_D]) state->etiPositionX += (int)(state->moveSpeed * state->delta);

	// poruszanie siê
	state->distance += state->etiSpeed * state->delta;

	// obliczanie FPS
	state->fpsTimer += state->delta;
	if (state->fpsTimer > FPS_REFRESH) {
		state->fps = state->frames * 2;
		state->frames = 0;
		state->fpsTimer -= FPS_REFRESH;
	};
	state->frames++;
}

void render(SDLContext* sdl, GameState* state) {
	char text[128];

	// rysowanie t³a
	SDL_FillRect(sdl->screen, NULL, state->czarny);

	// rysowanie gracza
	DrawSurface(sdl->screen, sdl->eti, (int)state->etiPositionX, (int)state->etiPositionY);

	// rysowanie info na górze
	DrawRectangle(sdl->screen, 4, 4, SCREEN_WIDTH - 8, 36, state->czerwony, state->niebieski);
	sprintf(text, "Szablon drugiego zadania, czas trwania = %.1lf s  %.0lf klatek / s", state->worldTime, state->fps);
	DrawString(sdl->screen, sdl->screen->w / 2 - strlen(text) * 8 / 2, 10, text, sdl->charset);
	sprintf(text, "Esc - wyjscie, \030 - przyspieszenie, \031 - zwolnienie");
	DrawString(sdl->screen, sdl->screen->w / 2 - strlen(text) * 8 / 2, 26, text, sdl->charset);

	// wysy³anie na ekran
	SDL_UpdateTexture(sdl->scrtex, NULL, sdl->screen->pixels, sdl->screen->pitch);
	SDL_RenderCopy(sdl->renderer, sdl->scrtex, NULL, NULL);
	SDL_RenderPresent(sdl->renderer);
}

//=================================
//       FUNCKCJE SETUP
//=================================

// inicjalizacja stanu gry oraz kolorów(dlatego przekazujemy surface ekranu)
void initGameState(GameState* state, const SDL_Surface* screen) {
	// Pozycje startowe
	state->etiPositionX = PLAYER_START_X_POS;
	state->etiPositionY = SCREEN_HEIGHT / 2 - PLAYER_START_X_POS;

	// Parametry ruchu
	state->etiSpeed = ETI_SPEED_VAL;
	state->moveSpeed = MOVE_SPEED; // pixele na sekundê
	state->distance = 0;

	// Czas
	state->worldTime = 0;
	state->t1 = SDL_GetTicks();
	state->frames = 0;
	state->fpsTimer = 0;
	state->fps = 0;

	// Flagi
	state->quit = 0;

	// Mapowanie kolorów (wymaga formatu ekranu, dlatego przekazujemy screen)
	state->czarny = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	state->zielony = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);
	state->czerwony = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	state->niebieski = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);
}

// funkcja inicjalizuj¹ca bibliotekê SDL
bool initSDL(SDLContext *sdl) {
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


	// wy³¹czenie widocznoœci kursora myszy
	SDL_ShowCursor(SDL_DISABLE);
}

// funkcja ³aduj¹ca zasoby (obrazy, czcionki, itp)
bool loadAssets(SDLContext* sdl) {
	// wczytanie czcionki cs8x8.bmp
	sdl->charset = SDL_LoadBMP("./cs8x8.bmp");
	if (sdl->charset == NULL) {
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		return 1;
	};
	SDL_SetColorKey(sdl->charset, true, 0x000000);

	// wczytanie obrazka (player)
	sdl->eti = SDL_LoadBMP("./eti.bmp");
	if (sdl->eti == NULL) {
		printf("SDL_LoadBMP(eti.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(sdl->charset);
		SDL_Quit();
		return 1;
	};
}

// funkcja czyszcz¹ca miejsce po SDL
void cleanup(SDLContext* sdl) {
	// Zwalnianie powierzchni (Surface)
	if (sdl->eti)     SDL_FreeSurface(sdl->eti);
	if (sdl->charset) SDL_FreeSurface(sdl->charset);
	if (sdl->screen)  SDL_FreeSurface(sdl->screen);

	// Zwalnianie tekstur, renderera i okna
	if (sdl->scrtex)   SDL_DestroyTexture(sdl->scrtex);
	if (sdl->renderer) SDL_DestroyRenderer(sdl->renderer);
	if (sdl->window)   SDL_DestroyWindow(sdl->window);

	// Wy³¹czenie SDL
	SDL_Quit();
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
	if (!loadAssets(&sdl)) {
		cleanup(&sdl);
		return 1;
	}

	// inicjalizacja stanu gry
	initGameState(&state, sdl.screen);

	// g³ówna pêtla gry
	while(!state.quit) {
		handleEvents(&state);
		updateGame(&state);
		render(&sdl, &state);
	};

	cleanup(&sdl);
	return 0;
};
