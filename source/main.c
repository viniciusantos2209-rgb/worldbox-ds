// WorldBox DS - mini simulador de deus para Nintendo DS (libnds / devkitARM)
// Tela de baixo: mundo (toque). Tela de cima: menu e populacao.

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MW 64          // largura do mapa em tiles
#define MH 48          // altura do mapa em tiles
#define TS 4           // tamanho do tile em pixels (64*4=256, 48*4=192)
#define MAXC 250       // maximo de criaturas

enum { T_DEEP, T_WATER, T_SAND, T_GRASS, T_FOREST, T_MOUNT, T_FIRE, T_ASH };
enum { C_NONE, C_HUMAN, C_SHEEP, C_WOLF };
enum { TOOL_DEEP, TOOL_WATER, TOOL_SAND, TOOL_GRASS, TOOL_FOREST, TOOL_MOUNT,
       TOOL_HUMAN, TOOL_SHEEP, TOOL_WOLF, TOOL_FIRE, TOOL_BOMB, TOOL_COUNT };

typedef struct { u8 x, y, type; s8 hp; u16 age; } Creature;

static const char *toolNames[TOOL_COUNT] = {
    "Agua funda", "Agua rasa", "Areia", "Grama", "Floresta", "Montanha",
    "Humano", "Ovelha", "Lobo", "Fogo", "Bomba"
};
static const int maxAge[4] = { 0, 4000, 2500, 3000 };
static const int popCap[4] = { 0, 60, 90, 25 };

static u8 map[MH][MW];
static u8 dirty[MH][MW];
static u8 occ[MH][MW];          // indice da criatura + 1 (0 = vazio)
static Creature cr[MAXC];
static int pop[4];
static u16 *fb;
static u32 frame = 0;

static int tool = TOOL_GRASS, brush = 1, paused = 0;

// ---------------------------------------------------------------- utilidades
static int sgn(int v) { return (v > 0) - (v < 0); }
static int inb(int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; }
static int passable(u8 t) { return t == T_SAND || t == T_GRASS || t == T_FOREST || t == T_ASH; }

static void killC(int i) {
    Creature *c = &cr[i];
    if (!c->type) return;
    occ[c->y][c->x] = 0;
    dirty[c->y][c->x] = 1;
    c->type = 0;
}

static void setTile(int x, int y, u8 t) {
    if (!inb(x, y) || map[y][x] == t) return;
    map[y][x] = t;
    dirty[y][x] = 1;
    if (occ[y][x] && !passable(t)) killC(occ[y][x] - 1);
}

static int spawn(int x, int y, int type) {
    if (!inb(x, y) || occ[y][x] || !passable(map[y][x])) return 0;
    for (int i = 0; i < MAXC; i++) {
        if (!cr[i].type) {
            cr[i].x = x; cr[i].y = y; cr[i].type = type; cr[i].age = 0;
            cr[i].hp = (type == C_WOLF) ? 6 : (type == C_HUMAN ? 5 : 3);
            occ[y][x] = i + 1;
            dirty[y][x] = 1;
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------- geracao
static void generate(void) {
    static u16 h[MH][MW];
    memset(h, 0, sizeof(h));
    memset(occ, 0, sizeof(occ));
    memset(cr, 0, sizeof(cr));

    for (int b = 0; b < 28; b++) {
        int cx = rand() % MW, cy = rand() % MH, r = 4 + rand() % 10, r2 = r * r;
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++) {
                if (!inb(x, y)) continue;
                int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (d < r2) h[y][x] += (r2 - d) * 16 / r2;
            }
    }
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            int v = h[y][x];
            u8 t;
            if (v == 0) t = T_DEEP;
            else if (v < 3) t = T_WATER;
            else if (v < 5) t = T_SAND;
            else if (v < 30) t = (rand() % 100 < 25) ? T_FOREST : T_GRASS;
            else t = T_MOUNT;
            map[y][x] = t;
            dirty[y][x] = 1;
        }
    for (int n = 0; n < 400; n++) {
        int x = rand() % MW, y = rand() % MH;
        if (pop[C_SHEEP] < 15 && spawn(x, y, C_SHEEP)) pop[C_SHEEP]++;
        else if (pop[C_HUMAN] < 8 && spawn(x, y, C_HUMAN)) pop[C_HUMAN]++;
        else if (pop[C_WOLF] < 2 && spawn(x, y, C_WOLF)) pop[C_WOLF]++;
    }
}

// ---------------------------------------------------------------- desenho
static void drawTile(int x, int y) {
    u8 b = map[y][x];
    u16 base = b | (b << 8), ca = base, cb = base;
    int o = occ[y][x];
    if (o) {
        u8 col = 7 + cr[o - 1].type;      // cores 8..10
        ca = b | (col << 8);
        cb = col | (b << 8);
    }
    u16 *p = fb + (y * TS) * 128 + x * 2;  // 256 px por linha = 128 u16
    p[0] = base;   p[1] = base;
    p[128] = ca;   p[129] = cb;
    p[256] = ca;   p[257] = cb;
    p[384] = base; p[385] = base;
}

static void drawDirty(void) {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (dirty[y][x]) { dirty[y][x] = 0; drawTile(x, y); }
}

// ---------------------------------------------------------------- simulacao
static int neighborIs(int x, int y, u8 t) {
    return (y > 0 && map[y - 1][x] == t) || (y < MH - 1 && map[y + 1][x] == t) ||
           (x > 0 && map[y][x - 1] == t) || (x < MW - 1 && map[y][x + 1] == t);
}

static void ignite(int x, int y) {
    if (!inb(x, y)) return;
    u8 t = map[y][x];
    if ((t == T_FOREST && rand() % 100 < 35) || (t == T_GRASS && rand() % 100 < 12))
        setTile(x, y, T_FIRE);
}

static void simTerrain(void) {
    for (int n = 0; n < 60; n++) {
        int x = rand() % MW, y = rand() % MH;
        u8 t = map[y][x];
        if (t == T_ASH && rand() % 60 == 0) setTile(x, y, T_GRASS);
        else if (t == T_GRASS && neighborIs(x, y, T_FOREST) && rand() % 40 == 0)
            setTile(x, y, T_FOREST);
    }
    if (frame % 6 == 0) {
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++)
                if (map[y][x] == T_FIRE) {
                    ignite(x + 1, y); ignite(x - 1, y); ignite(x, y + 1); ignite(x, y - 1);
                    if (rand() % 100 < 12) setTile(x, y, T_ASH);
                }
    }
}

static void moveC(int i, int nx, int ny) {
    Creature *c = &cr[i];
    if (!inb(nx, ny) || occ[ny][nx] || !passable(map[ny][nx])) return;
    occ[c->y][c->x] = 0;
    dirty[c->y][c->x] = 1;
    c->x = nx; c->y = ny;
    occ[ny][nx] = i + 1;
    dirty[ny][nx] = 1;
}

static void stepC(int i) {
    Creature *c = &cr[i];
    int x = c->x, y = c->y, dx = 0, dy = 0, hunting = 0;
    c->age++;
    if (c->age > maxAge[c->type]) { killC(i); return; }

    if (c->type == C_WOLF) {
        if (c->age % 10 == 0 && --c->hp <= 0) { killC(i); return; }   // fome
        int best = 999, bx = 0, by = 0, bo = 0;
        for (int yy = y - 5; yy <= y + 5; yy++)
            for (int xx = x - 5; xx <= x + 5; xx++) {
                if (!inb(xx, yy)) continue;
                int o = occ[yy][xx];
                if (!o || cr[o - 1].type == C_WOLF) continue;
                int d = (xx - x) * (xx - x) + (yy - y) * (yy - y);
                if (d < best) { best = d; bx = xx; by = yy; bo = o; }
            }
        if (bo) {
            if (best <= 2) {                       // ataque
                Creature *p = &cr[bo - 1];
                if (p->type == C_SHEEP) {
                    killC(bo - 1);
                    c->hp = (c->hp + 3 > 8) ? 8 : c->hp + 3;
                } else {
                    p->hp -= 2;
                    if (rand() % 3 == 0) c->hp--;
                    if (p->hp <= 0) { killC(bo - 1); c->hp = (c->hp + 3 > 8) ? 8 : c->hp + 3; }
                    if (c->hp <= 0) { killC(i); return; }
                }
                return;
            }
            dx = sgn(bx - x); dy = sgn(by - y); hunting = 1;
        }
    } else if (c->type == C_HUMAN) {
        if (c->hp < 5 && c->age % 40 == 0) c->hp++;
        for (int yy = y - 1; yy <= y + 1; yy++)
            for (int xx = x - 1; xx <= x + 1; xx++) {
                if (!inb(xx, yy)) continue;
                int o = occ[yy][xx];
                if (!o || o - 1 == i) continue;
                Creature *p = &cr[o - 1];
                if (p->type == C_WOLF && rand() % 2) {
                    p->hp -= 3;
                    if (p->hp <= 0) killC(o - 1);
                    return;
                }
                if (p->type == C_SHEEP && c->hp < 5) {
                    killC(o - 1); c->hp++;
                    return;
                }
            }
    }

    if (!hunting && rand() % 10 < 7) { dx = rand() % 3 - 1; dy = rand() % 3 - 1; }
    if (dx || dy) moveC(i, x + dx, y + dy);

    // reproducao
    int t = c->type;
    if (pop[t] < popCap[t] && c->age > 20 && rand() % 150 == 0 &&
        (t != C_WOLF || c->hp >= 6) && (t != C_SHEEP || map[c->y][c->x] == T_GRASS)) {
        if (spawn(c->x + rand() % 3 - 1, c->y + rand() % 3 - 1, t) && t == C_WOLF) c->hp -= 2;
    }
}

static void simCreatures(void) {
    memset(pop, 0, sizeof(pop));
    for (int i = 0; i < MAXC; i++) if (cr[i].type) pop[cr[i].type]++;
    for (int i = 0; i < MAXC; i++)
        if (cr[i].type && ((frame + i) & 7) == 0) stepC(i);
}

// ---------------------------------------------------------------- ferramentas
static void bomb(int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++)
        for (int dx = -4; dx <= 4; dx++) {
            int d = dx * dx + dy * dy, x = cx + dx, y = cy + dy;
            if (d > 16 || !inb(x, y)) continue;
            if (occ[y][x]) killC(occ[y][x] - 1);
            u8 t = map[y][x];
            if (t != T_DEEP && t != T_WATER) setTile(x, y, d <= 4 ? T_WATER : T_ASH);
        }
}

static void applyTool(int cx, int cy, int first) {
    if (tool == TOOL_BOMB) { if (first) bomb(cx, cy); return; }

    if (tool >= TOOL_HUMAN && tool <= TOOL_WOLF) {
        if (first || frame % 6 == 0) {
            int type = tool - TOOL_HUMAN + 1;   // C_HUMAN..C_WOLF
            spawn(cx + (rand() % (2 * brush + 1)) - brush,
                  cy + (rand() % (2 * brush + 1)) - brush, type);
        }
        return;
    }

    for (int dy = -brush; dy <= brush; dy++)
        for (int dx = -brush; dx <= brush; dx++) {
            if (dx * dx + dy * dy > brush * brush + brush) continue;
            int x = cx + dx, y = cy + dy;
            if (!inb(x, y)) continue;
            if (tool == TOOL_FIRE) {
                u8 t = map[y][x];
                if (t == T_GRASS || t == T_FOREST || t == T_SAND || t == T_ASH)
                    setTile(x, y, T_FIRE);
            } else {
                setTile(x, y, (u8)tool);   // TOOL_DEEP..TOOL_MOUNT == T_DEEP..T_MOUNT
            }
        }
}

// ---------------------------------------------------------------- HUD (tela de cima)
static void hud(void) {
    iprintf("\x1b[0;0H");
    iprintf(" === WORLDBOX DS ===\n\n");
    iprintf(" Ferramenta: %-12s\n", toolNames[tool]);
    iprintf(" Pincel: %d    %s\n\n", brush + 1, paused ? "[PAUSA]" : "       ");
    iprintf(" Humanos: %-4d\n Ovelhas: %-4d\n Lobos:   %-4d\n\n", pop[C_HUMAN], pop[C_SHEEP], pop[C_WOLF]);
    iprintf(" L/R ou setas: ferramenta\n");
    iprintf(" X / Y: aumentar/diminuir pincel\n");
    iprintf(" Toque: usar ferramenta\n");
    iprintf(" START: pausa\n");
    iprintf(" SELECT: novo mundo\n");
}

// ---------------------------------------------------------------- main
int main(void) {
    lcdMainOnBottom();                       // tela principal (mundo) embaixo = toque
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    fb = (u16 *)bgGetGfxPtr(bg);

    BG_PALETTE[0]  = RGB15(2, 6, 16);        // fundo (= agua funda; indice 0 e transparente)
    BG_PALETTE[1]  = RGB15(5, 12, 26);       // agua rasa
    BG_PALETTE[2]  = RGB15(28, 25, 14);      // areia
    BG_PALETTE[3]  = RGB15(8, 22, 6);        // grama
    BG_PALETTE[4]  = RGB15(3, 13, 4);        // floresta
    BG_PALETTE[5]  = RGB15(16, 16, 16);      // montanha
    BG_PALETTE[6]  = RGB15(31, 12, 0);       // fogo
    BG_PALETTE[7]  = RGB15(8, 7, 6);         // cinzas
    BG_PALETTE[8]  = RGB15(31, 24, 18);      // humano
    BG_PALETTE[9]  = RGB15(31, 31, 31);      // ovelha
    BG_PALETTE[10] = RGB15(12, 2, 2);        // lobo

    consoleDemoInit();                       // tela de cima: texto
    srand(time(NULL));
    generate();

    while (1) {
        scanKeys();
        u32 down = keysDown(), held = keysHeld();

        if (down & (KEY_R | KEY_RIGHT)) tool = (tool + 1) % TOOL_COUNT;
        if (down & (KEY_L | KEY_LEFT))  tool = (tool + TOOL_COUNT - 1) % TOOL_COUNT;
        if ((down & KEY_X) && brush < 4) brush++;
        if ((down & KEY_Y) && brush > 0) brush--;
        if (down & KEY_START) paused = !paused;
        if (down & KEY_SELECT) generate();

        if (held & KEY_TOUCH) {
            touchPosition t;
            touchRead(&t);
            applyTool(t.px / TS, t.py / TS, (down & KEY_TOUCH) != 0);
        }

        if (!paused) { simTerrain(); simCreatures(); }
        drawDirty();
        if (frame % 6 == 0) hud();

        frame++;
        swiWaitForVBlank();
    }
    return 0;
}
