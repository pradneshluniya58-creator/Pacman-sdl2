// pacman2.c — Pac-Man with classic scatter/chase schedule, ESC pause menu, SDL_ttf text,
// main menu (Play, Level 2 [Locked], Controls, Credits, Quit), and SDL_mixer music/SFX.
// Build: clang pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf sdl2_mixer) -o pacman2

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TILE 20
#define MAP_W 28
#define MAP_H 31
#define SCREEN_W (MAP_W*TILE)
#define SCREEN_H (MAP_H*TILE)

#define FPS 60
#define STEP_MS 110          // Pac-Man step timing
#define GHOST_MS 110         // Ghost step timing
#define FRIGHT_MS 6000       // frightened mode duration

typedef enum { MODE_SCATTER, MODE_CHASE, MODE_FRIGHT } GhostMode;
typedef enum { RED=0, PINK=1, BLUE=2, ORANGE=3 } GhostId;

// New: simple scene management
typedef enum { STATE_MAIN_MENU, STATE_CONTROLS, STATE_CREDITS, STATE_PLAYING } GameState;

typedef struct { int x,y; int dx,dy; int startx,starty; } Entity;
typedef struct { Entity e; GhostMode mode; Uint32 fright_timer; } Ghost;

// ===== Audio state =====
typedef enum { MS_NONE, MS_MENU, MS_GAME, MS_PAUSE, MS_VICTORY } MusicState;
static MusicState mus_state = MS_NONE;

static Mix_Music* mus_menu = NULL;     // Juhani Junkala — "Title Screen"
static Mix_Music* mus_game = NULL;     // FREE Action Chiptune Music Pack (choose one)
static Mix_Music* mus_pause = NULL;    // JRPG Pack 4 Calm — "Innocence"
static Mix_Music* mus_victory = NULL;  // Juhani Junkala — "Ending"
static Mix_Chunk* sfx_death = NULL;    // Short blip

static void audio_play(Mix_Music* m, int loops){
    if(!m) return;
    Mix_HaltMusic();
    Mix_PlayMusic(m, loops);
}
static void play_menu_music(void){ if(mus_state!=MS_MENU){ audio_play(mus_menu, -1); mus_state=MS_MENU; } }
static void play_game_music(void){ if(mus_state!=MS_GAME){ audio_play(mus_game, -1); mus_state=MS_GAME; } }
static void play_pause_music(void){ if(mus_state!=MS_PAUSE){ audio_play(mus_pause, -1); mus_state=MS_PAUSE; } }
static void play_victory_music(void){ if(mus_state!=MS_VICTORY){ audio_play(mus_victory, -1); mus_state=MS_VICTORY; } }
static void audio_stop(void){ Mix_HaltMusic(); mus_state = MS_NONE; }

// Asset paths (place files under assets/audio/)
#define PATH_MENU    "assets/audio/menu_title.wav"
#define PATH_GAME    "assets/audio/gameplay_action.mp3"   // pick one: e.g., "leaving home"
#define PATH_PAUSE   "assets/audio/pause_innocence.ogg"
#define PATH_VICTORY "assets/audio/victory_ending.wav"
#define PATH_DEATH   "assets/audio/sfx_death.ogg"

static bool audio_init(void){
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0){
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
        return false;
    }
    Mix_AllocateChannels(16);

    mus_menu   = Mix_LoadMUS(PATH_MENU);
    mus_game   = Mix_LoadMUS(PATH_GAME);
    mus_pause  = Mix_LoadMUS(PATH_PAUSE);
    mus_victory= Mix_LoadMUS(PATH_VICTORY);
    sfx_death  = Mix_LoadWAV(PATH_DEATH);

    if(!mus_menu)   SDL_Log("Load music (menu) failed: %s", Mix_GetError());
    if(!mus_game)   SDL_Log("Load music (game) failed: %s", Mix_GetError());
    if(!mus_pause)  SDL_Log("Load music (pause) failed: %s", Mix_GetError());
    if(!mus_victory)SDL_Log("Load music (victory) failed: %s", Mix_GetError());
    if(!sfx_death)  SDL_Log("Load sfx (death) failed: %s", Mix_GetError());

    // Start on menu track by default
    if(mus_menu) play_menu_music();
    return true;
}

static void audio_quit(void){
    audio_stop();
    if(mus_menu){ Mix_FreeMusic(mus_menu); mus_menu=NULL; }
    if(mus_game){ Mix_FreeMusic(mus_game); mus_game=NULL; }
    if(mus_pause){ Mix_FreeMusic(mus_pause); mus_pause=NULL; }
    if(mus_victory){ Mix_FreeMusic(mus_victory); mus_victory=NULL; }
    if(sfx_death){ Mix_FreeChunk(sfx_death); sfx_death=NULL; }
    Mix_CloseAudio();
}

// ===== Level map =====
static const char* LEVEL0[MAP_H] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##### ## #####.#     ",
    "     #.##          ##.#     ",
    "     #.## ###HH### ##.#     ",
    "######.## #      # ##.######",
    "      .   #  GG  #   .      ",
    "######.## #      # ##.######",
    "     #.## ######## ##.#     ",
    "     #.##          ##.#     ",
    "     #.## ######## ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o..##................##..o#",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
    "############################",
    "############################",
    "############################"
};

static char board[MAP_H][MAP_W];

// ===== Helpers =====
static inline bool in_bounds(int x,int y){ return x>=0 && x<MAP_W && y>=0 && y<MAP_H; }
static inline bool is_wall_at(int x,int y){ if(!in_bounds(x,y)) return true; return board[y][x]=='#'; }
static inline bool is_gate_at(int x,int y){ return in_bounds(x,y) && board[y][x]=='H'; }

static void reset_board(void){ for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) board[y][x]=LEVEL0[y][x]; }

static void wrap(Entity* e){ if(e->x<0) e->x=MAP_W-1; else if(e->x>=MAP_W) e->x=0; }

static bool passable_for_ghost(int x,int y){ if(!in_bounds(x,y)) return true; return board[y][x] != '#'; }
static bool passable_for_pac(int x,int y){ if(!in_bounds(x,y)) return true; char c=board[y][x]; if(c=='#'||c=='H') return false; return true; }

typedef struct { int x,y; } Point;

static Point next_step_bfs(Point src, Point dst, bool (*passable)(int,int)){
    static int qx[MAP_W*MAP_H], qy[MAP_W*MAP_H];
    static short px[MAP_W][MAP_H], py[MAP_W][MAP_H];
    static unsigned char vis[MAP_W][MAP_H];
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++){ vis[x][y]=0; px[x][y]=-1; py[x][y]=-1; }
    int head=0, tail=0;
    qx[tail]=src.x; qy[tail]=src.y; tail++; vis[src.x][src.y]=1;
    const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    while(head<tail){
        int x=qx[head], y=qy[head]; head++;
        if(x==dst.x && y==dst.y) break;
        for(int i=0;i<4;i++){
            int nx=x+dirs[i][0], ny=y+dirs[i][1];
            if(!in_bounds(nx,ny) && !(nx<0||nx>=MAP_W)) continue;
            if(nx<0||nx>=MAP_W){
                int wx=(nx<0)?MAP_W-1:0;
                if(!passable(wx,ny) || vis[wx][ny]) continue;
                vis[wx][ny]=1; px[wx][ny]=x; py[wx][ny]=y; qx[tail]=wx; qy[tail]=ny; tail++;
            }else{
                if(!passable(nx,ny) || vis[nx][ny]) continue;
                vis[nx][ny]=1; px[nx][ny]=x; py[nx][ny]=y; qx[tail]=nx; qy[tail]=ny; tail++;
            }
        }
    }
    int tx=dst.x, ty=dst.y;
    if(tx<0||tx>=MAP_W||ty<0||ty>=MAP_H) return src;
    if(!vis[tx][ty]) return src;
    while(!(px[tx][ty]==src.x && py[tx][ty]==src.y)){ int ntx=px[tx][ty], nty=py[tx][ty]; if(ntx==-1) break; tx=ntx; ty=nty; }
    Point step={tx,ty}; return step;
}

// Deterministic steering toward a target with tie-break U,L,D,R and anti-reverse
static void choose_dir_toward(Entity* e, Point tgt, bool (*pass)(int,int)) {
    const int DIRS[4][2] = { {0,-1}, {-1,0}, {0,1}, {1,0} }; // U, L, D, R
    int revx = -e->dx, revy = -e->dy;

    int viable = 0;
    for (int i = 0; i < 4; i++) {
        int ndx = DIRS[i][0], ndy = DIRS[i][1];
        if (ndx == revx && ndy == revy) continue;
        int nx = e->x + ndx, ny = e->y + ndy;
        if (pass(nx, ny)) viable++;
    }
    if (viable == 0) {
        int nx = e->x + revx, ny = e->y + revy;
        if (pass(nx, ny)) { e->dx = revx; e->dy = revy; }
        return;
    }
    int best_i = -1, best_d = 1<<30;
    for (int i = 0; i < 4; i++) {
        int ndx = DIRS[i][0], ndy = DIRS[i][1];
        if (ndx == revx && ndy == revy) continue;
        int nx = e->x + ndx, ny = e->y + ndy;
        if (!pass(nx, ny)) continue;
        int d = abs(nx - tgt.x) + abs(ny - tgt.y);
        if (d < best_d) { best_d = d; best_i = i; }
    }
    if (best_i >= 0) { e->dx = DIRS[best_i][0]; e->dy = DIRS[best_i][1]; }
}

static Point pac_ahead(Entity pac, int tiles){
    Point p={pac.x+pac.dx*tiles, pac.y+pac.dy*tiles};
    if(p.x<0)p.x=MAP_W-1; if(p.x>=MAP_W)p.x=0; return p;
}

// Classic targets: all ghosts scatter/chase per schedule; frightened ignores target.
static Point ghost_target(GhostId id, Entity pac, Ghost ghosts[4]){
    Ghost* g = &ghosts[id];
    if (g->mode == MODE_FRIGHT) return (Point){ g->e.x, g->e.y };
    Point corners[4]={{MAP_W-2,0},{1,0},{MAP_W-2,MAP_H-2},{1,MAP_H-2}};
    if (g->mode == MODE_SCATTER) return corners[id];

    // MODE_CHASE
    if (id == RED) { // Blinky
        return (Point){ pac.x, pac.y };
    } else if (id == PINK) { // Pinky
        Point a = pac_ahead(pac, 4); return a;
    } else if (id == BLUE) { // Inky
        Point p2 = pac_ahead(pac, 2);
        int vx = p2.x - ghosts[RED].e.x, vy = p2.y - ghosts[RED].e.y;
        return (Point){ p2.x + vx, p2.y + vy };
    } else { // ORANGE (Clyde)
        int dx = pac.x - g->e.x, dy = pac.y - g->e.y;
        int dist2 = dx*dx + dy*dy;
        if (dist2 >= 64) return (Point){ pac.x, pac.y };
        return corners[ORANGE];
    }
}

// ===== Text helpers =====
static void draw_text(SDL_Renderer* r, TTF_Font* font, const char* msg, int x, int y, SDL_Color color){
    if (!font || !msg) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, msg, color);
    if(!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if(tex){ SDL_RenderCopy(r, tex, NULL, &dst); SDL_DestroyTexture(tex); }
}

static void draw_text_center(SDL_Renderer* r, TTF_Font* font, const char* msg, int cx, int y, SDL_Color color){
    if(!font || !msg) return;
    int w=0,h=0; TTF_SizeUTF8(font, msg, &w, &h);
    draw_text(r, font, msg, cx - w/2, y, color);
}

static void draw_rect(SDL_Renderer*r,int x,int y,int w,int h, SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r,&rc);
}

// ===== ESC pause menu state =====
static bool esc_menu = false;
static int esc_sel = 0; // 0=Resume, 1=Retry, 2=Main Menu

static void render_esc_menu(SDL_Renderer* r, TTF_Font* font){
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int panel_w = 360, panel_h = 260;
    int px = SCREEN_W/2 - panel_w/2;
    int py = SCREEN_H/2 - panel_h/2;
    draw_rect(r, px, py, panel_w, panel_h, (SDL_Color){0,0,0,180});

    draw_text(r, font, "Paused", px + 130, py + 20, (SDL_Color){255,255,255,255});
    const char* items[3] = { "Resume", "Retry", "Main Menu" };
    for(int i=0;i<3;i++){
        SDL_Color col = (i==esc_sel)? (SDL_Color){255,215,0,255} : (SDL_Color){255,255,255,255};
        draw_text(r, font, items[i], px + 110, py + 70 + i*40, col);
    }
    draw_text(r, font, "made by pradnesh", px + 80, py + panel_h - 40, (SDL_Color){255,215,0,255});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ===== Main menu state =====
static GameState g_state = STATE_MAIN_MENU;
static int main_sel = 0;
static const char* MAIN_ITEMS[] = {
    "Play",
    "Level 2 [Locked]",
    "Controls",
    "Credits",
    "Quit"
};
static const int MAIN_COUNT = 5;
static Uint32 locked_msg_until = 0; // toast timer for locked level

static void render_main_menu(SDL_Renderer* r, TTF_Font* font){
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Title
    draw_text_center(r, font, "PAC-MAN", SCREEN_W/2, 60, (SDL_Color){255,255,0,255});

    // Menu panel
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int panel_w = 420, panel_h = 320;
    int px = SCREEN_W/2 - panel_w/2;
    int py = SCREEN_H/2 - panel_h/2;
    draw_rect(r, px, py, panel_w, panel_h, (SDL_Color){0,0,0,160});

    // Items
    for(int i=0;i<MAIN_COUNT;i++){
        SDL_Color col = (i==main_sel)? (SDL_Color){255,215,0,255} : (SDL_Color){255,255,255,255};
        draw_text_center(r, font, MAIN_ITEMS[i], SCREEN_W/2, py + 70 + i*40, col);
    }

    // Footer
    draw_text_center(r, font, "Use Up/Down or W/S, Enter to select • ESC to quit", SCREEN_W/2, py + panel_h - 40, (SDL_Color){180,180,180,255});

    // Locked toast
    Uint32 now = SDL_GetTicks();
    if(locked_msg_until && now < locked_msg_until){
        draw_rect(r, SCREEN_W/2-170, py-50, 340, 36, (SDL_Color){0,0,0,180});
        draw_text_center(r, font, "Locked — Coming Soon", SCREEN_W/2, py-44, (SDL_Color){255,100,100,255});
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_RenderPresent(r);
}

static void render_controls_screen(SDL_Renderer* r, TTF_Font* font){
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    draw_text_center(r, font, "Controls", SCREEN_W/2, 60, (SDL_Color){255,255,255,255});
    int y = 130;
    draw_text_center(r, font, "Move: Arrow Keys or W/A/S/D", SCREEN_W/2, y, (SDL_Color){200,200,200,255});
    y += 40;
    draw_text_center(r, font, "Pause: ESC (opens pause menu)", SCREEN_W/2, y, (SDL_Color){200,200,200,255});
    y += 40;
    draw_text_center(r, font, "Select/Confirm: Enter or Space", SCREEN_W/2, y, (SDL_Color){200,200,200,255});
    y += 40;
    draw_text_center(r, font, "Retry: R (from pause/end)", SCREEN_W/2, y, (SDL_Color){200,200,200,255});
    y += 60;
    draw_text_center(r, font, "Press ESC to go back", SCREEN_W/2, y, (SDL_Color){255,215,0,255});

    SDL_RenderPresent(r);
}

static void render_credits_screen(SDL_Renderer* r, TTF_Font* font){
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Make "Made by pradnesh" pop
    draw_text_center(r, font, "Credits", SCREEN_W/2, 60, (SDL_Color){255,255,255,255});
    int y = 110;
    draw_text_center(r, font, "Made by pradnesh", SCREEN_W/2, y, (SDL_Color){255,215,0,255}); y += 36;

    draw_text_center(r, font, "Libraries: SDL2, SDL_ttf, SDL_mixer", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 30;
    draw_text_center(r, font, "Classic scatter/chase schedule implemented", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 40;

    // Music credits
    draw_text_center(r, font, "Music Credits", SCREEN_W/2, y, (SDL_Color){255,255,255,255}); y += 30;
    draw_text_center(r, font, "Menu: \"Title Screen\" — Juhani Junkala (Retro Game Music Pack)", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 24;
    draw_text_center(r, font, "Gameplay: FREE Action Chiptune Music Pack — credit: PPEAK / Preston Peak (CC BY 4.0)", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 24;
    draw_text_center(r, font, "Pause: \"Innocence\" — Juhani Junkala (JRPG Pack 4 Calm)", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 24;
    draw_text_center(r, font, "Victory: \"Ending\" — Juhani Junkala (Retro Game Music Pack)", SCREEN_W/2, y, (SDL_Color){200,200,200,255}); y += 40;

    draw_text_center(r, font, "Press ESC to go back", SCREEN_W/2, y, (SDL_Color){255,215,0,255});

    SDL_RenderPresent(r);
}

// ===== Game rendering (unchanged visuals) =====
static void render_game(SDL_Renderer*r, Entity pac, Ghost ghosts[4], int score, int lives, bool game_won, bool over, bool paused, TTF_Font* font){
    SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderClear(r);
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++){
        char c=board[y][x];
        if(c=='#') draw_rect(r,x*TILE,y*TILE,TILE,TILE,(SDL_Color){0,0,160,255});
        else if(c=='.') draw_rect(r,x*TILE+TILE/2-2,y*TILE+TILE/2-2,4,4,(SDL_Color){255,215,0,255});
        else if(c=='o') draw_rect(r,x*TILE+TILE/2-5,y*TILE+TILE/2-5,10,10,(SDL_Color){255,255,255,255});
        else if(c=='H') draw_rect(r,x*TILE,y*TILE,TILE,4,(SDL_Color){80,80,80,255});
    }
    draw_rect(r,pac.x*TILE,pac.y*TILE,TILE,TILE,(SDL_Color){255,255,0,255});
    SDL_Color ghost_color[4]={{255,0,0,255},{255,105,180,255},{0,255,255,255},{255,165,0,255}};
    for(int i=0;i<4;i++){
        SDL_Color col = (ghosts[i].mode==MODE_FRIGHT)? (SDL_Color){0,0,255,255} : ghost_color[i];
        draw_rect(r,ghosts[i].e.x*TILE,ghosts[i].e.y*TILE,TILE,TILE,col);
    }
    int barw=(score%2000)*SCREEN_W/2000; draw_rect(r,0,SCREEN_H-6,barw,6,(SDL_Color){50,200,50,255});
    for(int i=0;i<lives;i++) draw_rect(r,i*14,0,12,6,(SDL_Color){255,255,0,255});

    // Overlay
    if(esc_menu){
        render_esc_menu(r, font);
    }else if(paused && (game_won || over)){
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        draw_rect(r, SCREEN_W/2-160, SCREEN_H/2-80, 320, 160, (SDL_Color){0,0,0,180});
        const char* title = game_won? "YOU WIN" : "GAME OVER";
        draw_text(r, font, title, SCREEN_W/2- (int)strlen(title)*8, SCREEN_H/2-50, (SDL_Color){255,255,255,255});
        draw_text(r, font, "Press Enter to retry", SCREEN_W/2-120, SCREEN_H/2+10, (SDL_Color){255,255,255,255});
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
    SDL_RenderPresent(r);
}

/* Classic global phase schedule (level 1 timing approximation):
   S7, C20, S7, C20, S5, C20, S5, C∞
   0 duration means "infinite" (stay in that mode). */
typedef struct { GhostMode mode; Uint32 dur_ms; } Phase;
static const Phase PHASES[] = {
    {MODE_SCATTER, 7000}, {MODE_CHASE, 20000},
    {MODE_SCATTER, 7000}, {MODE_CHASE, 20000},
    {MODE_SCATTER, 5000}, {MODE_CHASE, 20000},
    {MODE_SCATTER, 5000}, {MODE_CHASE, 0}
};
static int phase_idx = 0;
static Uint32 phase_start = 0;
static bool phase_inited = false;

static GhostMode current_phase_mode(void){ return PHASES[phase_idx].mode; }

// Pause/resume the schedule while any ghost is frightened
static void maybe_switch_modes(Ghost ghosts[4], Uint32 now){
    if(!phase_inited){ phase_inited=true; phase_start=now; phase_idx=0; }
    bool any_fright=false;
    for(int i=0;i<4;i++) if(ghosts[i].mode==MODE_FRIGHT) { any_fright=true; break; }
    if(any_fright) { return; }

    Uint32 dur = PHASES[phase_idx].dur_ms;
    if(dur==0) return;

    if(now - phase_start >= dur){
        if(phase_idx < (int)(sizeof(PHASES)/sizeof(PHASES[0])) - 1){
            phase_idx++;
            phase_start = now;
            GhostMode nm = PHASES[phase_idx].mode;
            for(int i=0;i<4;i++){
                if(ghosts[i].mode != MODE_FRIGHT) ghosts[i].mode = nm;
            }
        }
    }
}

static void set_frightened(Ghost ghosts[4]){
    Uint32 now=SDL_GetTicks();
    for(int i=0;i<4;i++){
        ghosts[i].mode = MODE_FRIGHT;
        ghosts[i].fright_timer = now + FRIGHT_MS;
    }
}

static void place_starts(Entity* pac, Ghost g[4]){
    pac->x=13; pac->y=20; pac->dx=-1; pac->dy=0; pac->startx=pac->x; pac->starty=pac->y;
    int found=0;
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++){
        if(LEVEL0[y][x]=='G' && found<4){
            g[found].e.x=x; g[found].e.y=y; g[found].e.startx=x; g[found].e.starty=y;
            g[found].e.dx=1; g[found].e.dy=0; g[found].mode=MODE_SCATTER; g[found].fright_timer=0; found++;
        }
    }
    while(found<4){
        g[found].e.x=13; g[found].e.y=14; g[found].e.startx=g[found].e.x; g[found].e.starty=g[found].e.y;
        g[found].e.dx=1; g[found].e.dy=0; g[found].mode=MODE_SCATTER; g[found].fright_timer=0; found++;
    }
    // Reset schedule to start at first SCATTER
    phase_inited=false;
}

static void reset_positions(Entity* pac, Ghost g[4]){
    pac->x=pac->startx; pac->y=pac->starty; pac->dx=-1; pac->dy=0;
    for(int i=0;i<4;i++){
        g[i].e.x=g[i].e.startx; g[i].e.y=g[i].e.starty; g[i].e.dx=1; g[i].e.dy=0;
    }
}

static int count_pellets(void){
    int c=0; for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) if(board[y][x]=='.'||board[y][x]=='o') c++; return c;
}

// Now implemented: switch to main menu scene
static void go_to_main_menu(void){
    g_state = STATE_MAIN_MENU;
    esc_menu = false;
    // Ensure menu music is playing
    play_menu_music();
}

// ===== main =====
int main(int argc, char** argv){
    (void)argc; (void)argv; srand((unsigned int)time(NULL));
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_AUDIO)!=0){ SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
    if(TTF_Init()!=0){ SDL_Log("TTF_Init failed: %s", TTF_GetError()); SDL_Quit(); return 1; }

    // Audio
    audio_init();

    TTF_Font* font = TTF_OpenFont("assets/DejaVuSans.ttf", 22);
    if(!font){ SDL_Log("TTF_OpenFont failed: %s", TTF_GetError()); }

    SDL_Window* win = SDL_CreateWindow("Pac-Man (C + SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    if(!win){ SDL_Log("CreateWindow failed: %s", SDL_GetError()); if(font) TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){ SDL_Log("CreateRenderer failed: %s", SDL_GetError()); SDL_DestroyWindow(win); if(font) TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }

    // Start on main menu instead of gameplay
    g_state = STATE_MAIN_MENU;
    play_menu_music();

    // Prepare gameplay state (will be reset on Play)
    reset_board();
    Entity pac; Ghost ghosts[4]; place_starts(&pac, ghosts);

    int lives=3, score=0, pellets=count_pellets();
    bool running=true, game_won=false, over=false, paused=false;
    Uint32 last_step=SDL_GetTicks(), last_ghost=last_step;
    int eat_streak=0;

    while(running){
        // Events
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=false;
            else if(e.type==SDL_KEYDOWN){
                SDL_Keycode k=e.key.keysym.sym;

                // Global: in menu/controls/credits, ESC often goes back or quits
                if(g_state == STATE_MAIN_MENU){
                    if(k==SDLK_ESCAPE){ running=false; continue; }
                    if(k==SDLK_UP || k==SDLK_w){ main_sel = (main_sel + MAIN_COUNT - 1)%MAIN_COUNT; continue; }
                    if(k==SDLK_DOWN || k==SDLK_s){ main_sel = (main_sel + 1)%MAIN_COUNT; continue; }
                    if(k==SDLK_RETURN || k==SDLK_KP_ENTER || k==SDLK_SPACE){
                        if(main_sel==0){
                            // Play
                            reset_board();
                            place_starts(&pac, ghosts);
                            lives=3; score=0; pellets=count_pellets();
                            game_won=false; over=false; paused=false; eat_streak=0;
                            last_step=last_ghost=SDL_GetTicks();
                            g_state = STATE_PLAYING;
                            // Switch to gameplay music
                            play_game_music();
                        }else if(main_sel==1){
                            // Locked level
                            locked_msg_until = SDL_GetTicks() + 1500;
                        }else if(main_sel==2){
                            g_state = STATE_CONTROLS;
                        }else if(main_sel==3){
                            g_state = STATE_CREDITS;
                        }else if(main_sel==4){
                            running=false;
                        }
                        continue;
                    }
                }else if(g_state == STATE_CONTROLS || g_state == STATE_CREDITS){
                    if(k==SDLK_ESCAPE || k==SDLK_BACKSPACE || k==SDLK_RETURN || k==SDLK_SPACE){
                        g_state = STATE_MAIN_MENU;
                        // Ensure menu music
                        play_menu_music();
                        continue;
                    }
                }else if(g_state == STATE_PLAYING){
                    // ===== In-game handling (original behavior) =====

                    // ESC toggles the pause menu unless the end screen is up
                    if(k==SDLK_ESCAPE){
                        if(esc_menu){
                            esc_menu=false;
                            paused = (game_won || over);
                            // Resume correct track
                            if(!paused && !game_won && !over) play_game_music();
                        }else if(!over && !game_won){
                            esc_menu=true;
                            paused=true;
                            esc_sel = 0;
                            // Switch to pause music
                            play_pause_music();
                        }
                        continue;
                    }

                    // If end screen is up (game over/win), allow retry via Enter/Space/R
                    if(paused && (over || game_won) && !esc_menu){
                        if(k==SDLK_RETURN || k==SDLK_KP_ENTER || k==SDLK_SPACE || k=='r'){
                            reset_board();
                            place_starts(&pac, ghosts);
                            lives=3; score=0; pellets=count_pellets();
                            game_won=false; over=false; paused=false; eat_streak=0;
                            last_step=last_ghost=SDL_GetTicks();
                            // Back to gameplay music
                            play_game_music();
                        }
                        continue;
                    }

                    // Handle navigation/selection when ESC menu is open
                    if(esc_menu){
                        if(k==SDLK_UP || k==SDLK_w){ esc_sel = (esc_sel + 3 - 1)%3; }
                        else if(k==SDLK_DOWN || k==SDLK_s){ esc_sel = (esc_sel + 1)%3; }
                        else if(k==SDLK_RETURN || k==SDLK_KP_ENTER || k==SDLK_SPACE){
                            if(esc_sel==0){
                                // Resume
                                esc_menu=false; paused=false;
                                play_game_music();
                            }else if(esc_sel==1){
                                // Retry
                                reset_board();
                                place_starts(&pac, ghosts);
                                lives=3; score=0; pellets=count_pellets();
                                game_won=false; over=false; paused=false; eat_streak=0;
                                last_step=last_ghost=SDL_GetTicks();
                                esc_menu=false;
                                play_game_music();
                            }else if(esc_sel==2){
                                // Main Menu
                                go_to_main_menu();
                                paused=false;
                                play_menu_music();
                            }
                        }else if(k=='r'){
                            // quick retry shortcut in menu
                            reset_board();
                            place_starts(&pac, ghosts);
                            lives=3; score=0; pellets=count_pellets();
                            game_won=false; over=false; paused=false; eat_streak=0;
                            last_step=last_ghost=SDL_GetTicks();
                            esc_menu=false;
                            play_game_music();
                        }
                        continue;
                    }

                    // Gameplay input (only when not paused by menu or end screen)
                    if(!paused){
                        if(k==SDLK_LEFT || k==SDLK_a){ ((Entity*)&pac)->dx=-1; ((Entity*)&pac)->dy=0; }
                        else if(k==SDLK_DOWN || k==SDLK_s){ ((Entity*)&pac)->dx=0; ((Entity*)&pac)->dy=1; }
                        else if(k==SDLK_UP || k==SDLK_w){ ((Entity*)&pac)->dx=0; ((Entity*)&pac)->dy=-1; }
                        else if(k==SDLK_RIGHT || k==SDLK_d){ ((Entity*)&pac)->dx=1; ((Entity*)&pac)->dy=0; }
                    }
                }
            }
        }

        Uint32 now=SDL_GetTicks();

        // ===== Scene update + render =====
        if(g_state == STATE_PLAYING){
            if(!paused) maybe_switch_modes(ghosts, now);

            // Pac-Man step
            if(now - last_step >= STEP_MS && !paused){
                last_step=now;
                int nx=pac.x+pac.dx, ny=pac.y+pac.dy;
                if(passable_for_pac(nx,ny)){
                    pac.x=nx; pac.y=ny; wrap(&pac);
                    char c = in_bounds(pac.x,pac.y)? board[pac.y][pac.x] : ' ';
                    if(c=='.'){ board[pac.y][pac.x]=' '; score+=10; pellets--; eat_streak=0; }
                    else if(c=='o'){ board[pac.y][pac.x]=' '; score+=50; pellets--; eat_streak=0; set_frightened(ghosts); }
                    if(pellets<=0){
                        game_won=true; paused=true;
                        play_victory_music();
                    }
                }
            }

            // Ghost step
            if(now - last_ghost >= GHOST_MS && !paused){
                last_ghost=now;
                for(int i=0;i<4;i++){
                    // frightened expiry: return to current schedule phase
                    if(ghosts[i].mode==MODE_FRIGHT && now>=ghosts[i].fright_timer){
                        ghosts[i].mode = current_phase_mode();
                    }

                    if(ghosts[i].mode==MODE_FRIGHT){
                        // random only in frightened
                        static const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
                        int idx = rand()%4;
                        ghosts[i].e.dx = dirs[idx][0]; ghosts[i].e.dy = dirs[idx][1];
                    }else{
                        Point src={ghosts[i].e.x,ghosts[i].e.y};
                        Point tgt=ghost_target((GhostId)i, pac, ghosts);
                        if(tgt.x<0)tgt.x=0; if(tgt.x>=MAP_W)tgt.x=MAP_W-1;
                        if(tgt.y<0)tgt.y=0; if(tgt.y>=MAP_H)tgt.y=MAP_H-1;

                        Point step=next_step_bfs(src,tgt,passable_for_ghost);
                        int ndx=step.x-ghosts[i].e.x, ndy=step.y-ghosts[i].e.y;
                        if(ndx||ndy){
                            ghosts[i].e.dx = (ndx>0)?1:(ndx<0)?-1:0;
                            ghosts[i].e.dy = (ndy>0)?1:(ndy<0)?-1:0;
                        }else{
                            choose_dir_toward(&ghosts[i].e, tgt, passable_for_ghost);
                        }
                    }

                    ghosts[i].e.x += ghosts[i].e.dx;
                    ghosts[i].e.y += ghosts[i].e.dy;
                    if(ghosts[i].e.x<0) ghosts[i].e.x=MAP_W-1; if(ghosts[i].e.x>=MAP_W) ghosts[i].e.x=0;
                    if(!passable_for_ghost(ghosts[i].e.x,ghosts[i].e.y)){
                        ghosts[i].e.x -= ghosts[i].e.dx;
                        ghosts[i].e.y -= ghosts[i].e.dy;
                        ghosts[i].e.dx = -ghosts[i].e.dx;
                        ghosts[i].e.dy = -ghosts[i].e.dy;
                    }
                }

                // Collisions
                for(int i=0;i<4;i++){
                    if(pac.x==ghosts[i].e.x && pac.y==ghosts[i].e.y){
                        if(ghosts[i].mode==MODE_FRIGHT){
                            int pts = 200 << (eat_streak>3?3:eat_streak);
                            score += pts; eat_streak++;
                            ghosts[i].e.x=ghosts[i].e.startx; ghosts[i].e.y=ghosts[i].e.starty;
                            ghosts[i].mode = current_phase_mode();
                            ghosts[i].fright_timer=0;
                        }else{
                            lives--;
                            // Play death sfx
                            if(sfx_death) Mix_PlayChannel(-1, sfx_death, 0);
                            if(lives<=0){
                                over=true; paused=true;
                                // Optional: switch to pause music for end screen; keep victory only for wins
                                play_pause_music();
                            }
                            reset_positions(&pac, ghosts);
                            break;
                        }
                    }
                }
            }

            render_game(ren, pac, ghosts, score, lives, game_won, over, paused, font);
        }else if(g_state == STATE_MAIN_MENU){
            // Keep menu music rolling
            if(mus_state!=MS_MENU) play_menu_music();
            render_main_menu(ren, font);
        }else if(g_state == STATE_CONTROLS){
            render_controls_screen(ren, font);
        }else if(g_state == STATE_CREDITS){
            render_credits_screen(ren, font);
        }

        SDL_Delay(1000/FPS);
    }

    if(font) TTF_CloseFont(font);
    audio_quit();
    TTF_Quit();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
