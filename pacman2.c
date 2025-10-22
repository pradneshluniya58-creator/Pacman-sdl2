// pacman2.c — Pac-Man with classic scatter/chase schedule, ESC pause menu, and SDL_ttf text.
// Build: clang pacman2.c $(pkg-config --cflags --libs sdl2 sdl2_ttf) -o pacman2

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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

typedef struct { int x,y; int dx,dy; int startx,starty; } Entity;
typedef struct { Entity e; GhostMode mode; Uint32 fright_timer; } Ghost;

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

// Text helper
static void draw_text(SDL_Renderer* r, TTF_Font* font, const char* msg, int x, int y, SDL_Color color){
    if (!font) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, msg, color);
    if(!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if(tex){ SDL_RenderCopy(r, tex, NULL, &dst); SDL_DestroyTexture(tex); }
}

static void draw_rect(SDL_Renderer*r,int x,int y,int w,int h, SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r,&rc);
}

// ===== ESC menu state =====
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
    draw_text(r, font, "made by pradnesh", px + 80, py + panel_h - 40, (SDL_Color){180,180,180,255});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void render(SDL_Renderer*r, Entity pac, Ghost ghosts[4], int score, int lives, bool game_won, bool over, bool paused, TTF_Font* font){
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

    // Priority: ESC menu overlay first (when active), otherwise end overlay when paused by win/over
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

// Pause/resume the schedule while any ghost is frightened (so timer doesn't tick down under FRIGHT)
static void maybe_switch_modes(Ghost ghosts[4], Uint32 now){
    if(!phase_inited){ phase_inited=true; phase_start=now; phase_idx=0; }

    // If any ghost is frightened, pause the schedule timer
    bool any_fright=false;
    for(int i=0;i<4;i++) if(ghosts[i].mode==MODE_FRIGHT) { any_fright=true; break; }
    if(any_fright) { return; } // timer paused

    Uint32 dur = PHASES[phase_idx].dur_ms;
    if(dur==0) return; // infinite current phase (final chase)

    if(now - phase_start >= dur){
        // advance phase
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

// Placeholder hook for future main menu
static void go_to_main_menu(void){
    SDL_Log("Main Menu selected (TODO: implement main menu screen)"); // stub
}

int main(int argc, char** argv){
    (void)argc; (void)argv; srand((unsigned int)time(NULL));
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
    if(TTF_Init()!=0){ SDL_Log("TTF_Init failed: %s", TTF_GetError()); SDL_Quit(); return 1; }

    TTF_Font* font = TTF_OpenFont("assets/DejaVuSans.ttf", 22);
    if(!font){ SDL_Log("TTF_OpenFont failed: %s", TTF_GetError()); }

    SDL_Window* win = SDL_CreateWindow("Pac-Man (C + SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    if(!win){ SDL_Log("CreateWindow failed: %s", SDL_GetError()); if(font) TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){ SDL_Log("CreateRenderer failed: %s", SDL_GetError()); SDL_DestroyWindow(win); if(font) TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }

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

                // ESC toggles the pause menu unless the end screen is up
                if(k==SDLK_ESCAPE){
                    if(esc_menu){
                        esc_menu=false;
                        paused = (game_won || over);
                    }else if(!over && !game_won){
                        esc_menu=true;
                        paused=true;
                        esc_sel = 0;
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
                        }else if(esc_sel==1){
                            // Retry
                            reset_board();
                            place_starts(&pac, ghosts);
                            lives=3; score=0; pellets=count_pellets();
                            game_won=false; over=false; paused=false; eat_streak=0;
                            last_step=last_ghost=SDL_GetTicks();
                            esc_menu=false;
                        }else if(esc_sel==2){
                            // Main Menu (stub)
                            go_to_main_menu();
                            // Keep the menu open for now, ready to switch to main menu scene in the future
                        }
                    }else if(k=='r'){
                        // quick retry shortcut in menu
                        reset_board();
                        place_starts(&pac, ghosts);
                        lives=3; score=0; pellets=count_pellets();
                        game_won=false; over=false; paused=false; eat_streak=0;
                        last_step=last_ghost=SDL_GetTicks();
                        esc_menu=false;
                    }
                    // Do not process gameplay input while menu is open
                    continue;
                }

                // Gameplay input (only when not paused by menu or end screen)
                if(!paused){
                    if(k==SDLK_LEFT || k==SDLK_a){ pac.dx=-1; pac.dy=0; }
                    else if(k==SDLK_DOWN || k==SDLK_s){ pac.dx=0; pac.dy=1; }
                    else if(k==SDLK_UP || k==SDLK_w){ pac.dx=0; pac.dy=-1; }
                    else if(k==SDLK_RIGHT || k==SDLK_d){ pac.dx=1; pac.dy=0; }
                }
            }
        }

        Uint32 now=SDL_GetTicks();
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
                if(pellets<=0){ game_won=true; paused=true; }
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
                        if(lives<=0){ over=true; paused=true; }
                        reset_positions(&pac, ghosts);
                        break;
                    }
                }
            }
        }

        render(ren, pac, ghosts, score, lives, game_won, over, paused, font);
        SDL_Delay(1000/FPS);
    }

    if(font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
