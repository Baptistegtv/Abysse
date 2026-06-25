// ============================================================
//  ABYSSE - mange pour grandir
//  Jeu original pour Nintendo 3DS Homebrew (3dsx)
//  devkitARM + libctru + citro2d
// ============================================================
//
//  Principe :
//  - On controle un petit poisson-clown.
//  - On peut manger tout ce qui est plus petit que nous.
//  - Manger fait grandir, ce qui permet de manger des choses
//    de plus en plus grosses.
//  - Plusieurs niveaux : Recif -> Haute mer -> Fosse abyssale
//  - A chaque niveau franchi, le joueur debloque une forme
//    plus grande et plus impressionnante.
//
// ============================================================

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---------------- Constantes generales ----------------------

#define SCREEN_W 400
#define SCREEN_H 240

#define WORLD_W 2000
#define WORLD_H 1200

#define MAX_ENTITIES 60
#define MAX_PARTICLES 40

#define NB_NIVEAUX 3

// ---------------- Types -------------------------------------

typedef enum {
    ETAT_TITRE,
    ETAT_JEU,
    ETAT_TRANSITION,
    ETAT_GAMEOVER,
    ETAT_VICTOIRE
} EtatJeu;

typedef enum {
    FORME_CLOWN = 0,   // petit poisson-clown (depart)
    FORME_BALLON,      // poisson-ballon (niveau 2)
    FORME_TITAN         // titan des abysses (niveau 3)
} FormeJoueur;

typedef struct {
    float x, y;
    float vx, vy;
    float taille;       // rayon en pixels
    int actif;
    float r, g, b;       // couleur de base
    float anim;          // pour petites animations (nage)
} Entite;

typedef struct {
    float x, y;
    float vx, vy;
    float vie;
    float r, g, b;
} Particule;

// ---------------- Variables globales --------------------------

static Entite entites[MAX_ENTITIES];
static Particule particules[MAX_PARTICLES];

static EtatJeu etat = ETAT_TITRE;
static int niveau = 0; // 0,1,2

static float joueurX, joueurY;
static float joueurTaille;
static float joueurVx, joueurVy;
static FormeJoueur formeJoueur = FORME_CLOWN;
static float anim_nage = 0.0f;
static int direction_droite = 1;

static float camX, camY;

static int score = 0;
static float chronoTransition = 0.0f;

static u32 clrFond[NB_NIVEAUX];
static u32 clrFondBas[NB_NIVEAUX];

static C3D_RenderTarget* topTarget;
static C3D_RenderTarget* botTarget;

static C2D_TextBuf textBuf;

// seuils de taille pour debloquer la forme suivante
static const float SEUIL_NIVEAU[NB_NIVEAUX] = { 55.0f, 90.0f, 140.0f };

// ---------------- Utilitaires ----------------------------------

static float frand(float a, float b) {
    return a + ((float)rand() / (float)RAND_MAX) * (b - a);
}

static float distance(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

static void ajouterParticule(float x, float y, float r, float g, float b) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particules[i].vie <= 0.0f) {
            particules[i].x = x;
            particules[i].y = y;
            particules[i].vx = frand(-1.5f, 1.5f);
            particules[i].vy = frand(-1.5f, 1.5f);
            particules[i].vie = 1.0f;
            particules[i].r = r; particules[i].g = g; particules[i].b = b;
            return;
        }
    }
}

static void majParticules(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particules[i].vie > 0.0f) {
            particules[i].x += particules[i].vx;
            particules[i].y += particules[i].vy;
            particules[i].vie -= 0.03f;
        }
    }
}

// ---------------- Gestion des entites (proies / obstacles) -----

// Cree une entite mangeable ou non selon sa taille par rapport au joueur
static void spawnEntite(int idx) {
    Entite *e = &entites[idx];
    e->actif = 1;
    e->x = frand(camX - 100, camX + SCREEN_W + 100);
    e->y = frand(camY - 100, camY + SCREEN_H + 100);

    // bornage dans le monde
    if (e->x < 0) e->x = 0;
    if (e->x > WORLD_W) e->x = WORLD_W;
    if (e->y < 0) e->y = 0;
    if (e->y > WORLD_H) e->y = WORLD_H;

    // distribution de tailles : la plupart plus petites que le joueur,
    // certaines plus grosses (danger / futur objectif)
    float roll = frand(0.0f, 1.0f);
    if (roll < 0.65f) {
        // plus petit que le joueur : mangeable
        e->taille = frand(4.0f, joueurTaille * 0.85f);
    } else if (roll < 0.9f) {
        // a peu pres pareil
        e->taille = frand(joueurTaille * 0.8f, joueurTaille * 1.3f);
    } else {
        // plus gros : danger pour l'instant
        e->taille = frand(joueurTaille * 1.3f, joueurTaille * 2.2f);
    }
    if (e->taille < 3.0f) e->taille = 3.0f;

    e->vx = frand(-0.6f, 0.6f);
    e->vy = frand(-0.6f, 0.6f);
    e->anim = frand(0.0f, 6.28f);

    // couleur selon niveau (variete visuelle)
    if (niveau == 0) {
        // recif : tons jaunes/oranges/verts (poissons, crevettes)
        e->r = frand(0.6f, 1.0f);
        e->g = frand(0.4f, 0.9f);
        e->b = frand(0.1f, 0.4f);
    } else if (niveau == 1) {
        // haute mer : tons bleus/gris (thons, requins)
        e->r = frand(0.2f, 0.5f);
        e->g = frand(0.3f, 0.6f);
        e->b = frand(0.5f, 0.9f);
    } else {
        // fosse abyssale : tons violets/sombres (créatures étranges)
        e->r = frand(0.3f, 0.6f);
        e->g = frand(0.0f, 0.3f);
        e->b = frand(0.4f, 0.8f);
    }
}

static void initEntites(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entites[i].actif = 0;
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        spawnEntite(i);
    }
}

static void majEntites(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entite *e = &entites[i];
        if (!e->actif) continue;

        e->anim += 0.08f;
        e->x += e->vx + sinf(e->anim) * 0.3f;
        e->y += e->vy + cosf(e->anim * 0.7f) * 0.2f;

        // rebond sur les bords du monde
        if (e->x < 0) { e->x = 0; e->vx *= -1; }
        if (e->x > WORLD_W) { e->x = WORLD_W; e->vx *= -1; }
        if (e->y < 0) { e->y = 0; e->vy *= -1; }
        if (e->y > WORLD_H) { e->y = WORLD_H; e->vy *= -1; }

        // collision avec le joueur
        float d = distance(e->x, e->y, joueurX, joueurY);
        if (d < (e->taille + joueurTaille) * 0.55f) {
            if (e->taille < joueurTaille * 0.95f) {
                // le joueur mange l'entite
                float gain = e->taille * 0.18f;
                joueurTaille += gain;
                score += (int)(e->taille * 2.0f);

                for (int p = 0; p < 5; p++) {
                    ajouterParticule(e->x, e->y, e->r, e->g, e->b);
                }

                // respawn ailleurs, plus loin de la camera
                spawnEntite(i);
            } else if (e->taille > joueurTaille * 1.15f) {
                // l'entite est trop grosse : le joueur perd un peu de taille
                joueurTaille -= 4.0f;
                if (joueurTaille < 8.0f) joueurTaille = 8.0f;
                // on repousse le joueur
                float dx = joueurX - e->x, dy = joueurY - e->y;
                float len = sqrtf(dx*dx + dy*dy) + 0.001f;
                joueurX += (dx/len) * 18.0f;
                joueurY += (dy/len) * 18.0f;
            }
        }
    }
}

// ---------------- Joueur ----------------------------------------

static void resetJoueur(void) {
    joueurX = WORLD_W * 0.15f;
    joueurY = WORLD_H * 0.5f;
    joueurVx = 0; joueurVy = 0;
    joueurTaille = 14.0f;
    formeJoueur = FORME_CLOWN;
    score = 0;
}

static void majCamera(void) {
    camX = joueurX - SCREEN_W / 2.0f;
    camY = joueurY - SCREEN_H / 2.0f;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > WORLD_W - SCREEN_W) camX = WORLD_W - SCREEN_W;
    if (camY > WORLD_H - SCREEN_H) camY = WORLD_H - SCREEN_H;
}

// gere le changement de forme + le changement de niveau quand on grandit assez
static void verifierProgression(void) {
    if (niveau < NB_NIVEAUX - 1 && joueurTaille >= SEUIL_NIVEAU[niveau]) {
        niveau++;
        formeJoueur = (FormeJoueur)niveau; // CLOWN(0) -> BALLON(1) -> TITAN(2)
        etat = ETAT_TRANSITION;
        chronoTransition = 2.2f;
        initEntites();
    } else if (niveau == NB_NIVEAUX - 1 && joueurTaille >= SEUIL_NIVEAU[NB_NIVEAUX - 1] * 1.6f) {
        etat = ETAT_VICTOIRE;
    }
}

// ---------------- Entrees ----------------------------------------

static void gererEntrees(void) {
    hidScanInput();
    u32 kHeld = hidKeysHeld();
    circlePosition cp;
    hidCircleRead(&cp);

    float ax = 0, ay = 0;

    // stick circulaire (3DS)
    if (abs(cp.dx) > 10 || abs(cp.dy) > 10) {
        ax = cp.dx / 156.0f;
        ay = -cp.dy / 156.0f;
    }

    // croix directionnelle en complement
    if (kHeld & KEY_DUP)    ay = -1.0f;
    if (kHeld & KEY_DDOWN)  ay = 1.0f;
    if (kHeld & KEY_DLEFT)  ax = -1.0f;
    if (kHeld & KEY_DRIGHT) ax = 1.0f;

    float vitesse = 1.6f + joueurTaille * 0.012f; // un peu plus rapide en grandissant
    joueurVx += ax * 0.25f;
    joueurVy += ay * 0.25f;

    // friction
    joueurVx *= 0.9f;
    joueurVy *= 0.9f;

    // clamp vitesse
    float vlen = sqrtf(joueurVx*joueurVx + joueurVy*joueurVy);
    float vmax = vitesse;
    if (vlen > vmax) {
        joueurVx = joueurVx / vlen * vmax;
        joueurVy = joueurVy / vlen * vmax;
    }

    if (fabsf(joueurVx) > 0.1f) direction_droite = (joueurVx > 0);

    joueurX += joueurVx;
    joueurY += joueurVy;

    // bornage dans le monde
    if (joueurX < joueurTaille) joueurX = joueurTaille;
    if (joueurY < joueurTaille) joueurY = joueurTaille;
    if (joueurX > WORLD_W - joueurTaille) joueurX = WORLD_W - joueurTaille;
    if (joueurY > WORLD_H - joueurTaille) joueurY = WORLD_H - joueurTaille;

    anim_nage += 0.15f + vlen * 0.05f;
}

// ---------------- Dessin ------------------------------------------

static void dessinerPoisson(float sx, float sy, float taille, float r, float g, float b, int regarde_droite, float phase) {
    u32 couleurCorps = C2D_Color32f(r, g, b, 1.0f);
    u32 couleurVentre = C2D_Color32f(1.0f, 1.0f, 1.0f, 1.0f);
    u32 couleurNageoire = C2D_Color32f(r*0.7f, g*0.7f, b*0.7f, 1.0f);

    float balance = sinf(phase) * taille * 0.08f;

    // corps (cercle/ovale)
    C2D_DrawEllipseSolid(sx - taille*0.5f, sy - taille*0.35f + balance, 0.0f, taille, taille*0.7f, couleurCorps);

    // ventre clair
    C2D_DrawEllipseSolid(sx - taille*0.3f, sy - taille*0.05f + balance, 0.0f, taille*0.6f, taille*0.35f, couleurVentre);

    // queue (triangle)
    float qx = regarde_droite ? (sx - taille*0.95f) : (sx + taille*0.95f - taille*0.5f);
    float dir = regarde_droite ? -1.0f : 1.0f;
    C2D_DrawTriangle(
        sx + dir * taille*0.45f, sy - taille*0.35f + balance, couleurNageoire,
        sx + dir * (taille*0.45f + taille*0.5f), sy - taille*0.7f + balance, couleurNageoire,
        sx + dir * (taille*0.45f + taille*0.5f), sy + taille*0.0f + balance, couleurNageoire,
        0.0f
    );

    // oeil
    float ex = regarde_droite ? sx + taille*0.15f : sx - taille*0.15f;
    C2D_DrawEllipseSolid(ex - taille*0.12f, sy - taille*0.45f + balance, 0.0f, taille*0.16f, taille*0.16f, C2D_Color32f(1,1,1,1));
    C2D_DrawEllipseSolid(ex - taille*0.06f, sy - taille*0.42f + balance, 0.0f, taille*0.08f, taille*0.08f, C2D_Color32f(0,0,0,1));

    // rayures si forme clown
}

static void dessinerJoueur(void) {
    float sx = joueurX - camX;
    float sy = joueurY - camY;

    if (formeJoueur == FORME_CLOWN) {
        dessinerPoisson(sx, sy, joueurTaille, 1.0f, 0.45f, 0.05f, direction_droite, anim_nage);
        // rayures blanches
        u32 blanc = C2D_Color32f(1,1,1,0.9f);
        C2D_DrawEllipseSolid(sx - joueurTaille*0.55f, sy - joueurTaille*0.5f, 0.0f, joueurTaille*0.12f, joueurTaille*0.55f, blanc);
        C2D_DrawEllipseSolid(sx - joueurTaille*0.05f, sy - joueurTaille*0.5f, 0.0f, joueurTaille*0.12f, joueurTaille*0.55f, blanc);
    } else if (formeJoueur == FORME_BALLON) {
        dessinerPoisson(sx, sy, joueurTaille, 0.85f, 0.75f, 0.15f, direction_droite, anim_nage);
        // petites pointes (style poisson-ballon)
        u32 pointe = C2D_Color32f(0.6f, 0.5f, 0.1f, 1.0f);
        for (int i = 0; i < 8; i++) {
            float ang = i * 0.785f + anim_nage*0.2f;
            float px = sx + cosf(ang) * joueurTaille * 0.65f;
            float py = sy + sinf(ang) * joueurTaille * 0.55f;
            C2D_DrawEllipseSolid(px - 2, py - 2, 0.0f, 4, 4, pointe);
        }
    } else {
        // TITAN : forme imposante, sombre, presque mythique
        dessinerPoisson(sx, sy, joueurTaille, 0.25f, 0.15f, 0.45f, direction_droite, anim_nage);
        u32 lueur = C2D_Color32f(0.6f, 0.9f, 1.0f, 0.5f);
        C2D_DrawEllipseSolid(sx - joueurTaille*0.6f, sy - joueurTaille*0.45f, 0.0f, joueurTaille*0.2f, joueurTaille*0.2f, lueur);
    }
}

static void dessinerEntites(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entite *e = &entites[i];
        if (!e->actif) continue;
        float sx = e->x - camX;
        float sy = e->y - camY;
        if (sx < -e->taille*2 || sx > SCREEN_W + e->taille*2) continue;
        if (sy < -e->taille*2 || sy > SCREEN_H + e->taille*2) continue;

        int regarde = (e->vx >= 0);
        dessinerPoisson(sx, sy, e->taille, e->r, e->g, e->b, regarde, e->anim);
    }
}

static void dessinerParticules(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particule *p = &particules[i];
        if (p->vie <= 0.0f) continue;
        float sx = p->x - camX, sy = p->y - camY;
        u32 c = C2D_Color32f(p->r, p->g, p->b, p->vie);
        C2D_DrawEllipseSolid(sx - 3, sy - 3, 0.0f, 6, 6, c);
    }
}

static void dessinerFondBulles(void) {
    // petites bulles decoratives qui montent (purement visuel, basees sur le temps)
    static float t = 0.0f;
    t += 0.02f;
    u32 bulle = C2D_Color32f(1,1,1,0.25f);
    for (int i = 0; i < 14; i++) {
        float bx = fmodf(i * 73.0f - camX*0.3f, SCREEN_W + 40) ;
        float by = fmodf(SCREEN_H - fmodf(t*40.0f + i*37.0f, SCREEN_H + 40), SCREEN_H + 40) - 20;
        if (bx < 0) bx += SCREEN_W + 40;
        C2D_DrawEllipseSolid(bx, by, 0.0f, 3 + (i%3), 3 + (i%3), bulle);
    }
}

static const char* nomNiveau(int n) {
    if (n == 0) return "Le Recif";
    if (n == 1) return "La Haute Mer";
    return "La Fosse Abyssale";
}

static const char* nomForme(FormeJoueur f) {
    if (f == FORME_CLOWN) return "Poisson-clown";
    if (f == FORME_BALLON) return "Poisson-ballon geant";
    return "Titan des Abysses";
}

// ---------------- Boucle principale -------------------------------

int main(int argc, char **argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    textBuf = C2D_TextBufNew(4096);

    clrFond[0] = C2D_Color32f(0.10f, 0.55f, 0.65f, 1.0f);
    clrFond[1] = C2D_Color32f(0.05f, 0.30f, 0.55f, 1.0f);
    clrFond[2] = C2D_Color32f(0.03f, 0.05f, 0.20f, 1.0f);
    clrFondBas[0] = C2D_Color32f(0.06f, 0.35f, 0.45f, 1.0f);
    clrFondBas[1] = C2D_Color32f(0.02f, 0.15f, 0.30f, 1.0f);
    clrFondBas[2] = C2D_Color32f(0.01f, 0.02f, 0.10f, 1.0f);

    srand((unsigned int)svcGetSystemTick());

    resetJoueur();
    niveau = 0;
    initEntites();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        switch (etat) {
            case ETAT_TITRE:
                if (kDown & (KEY_A | KEY_TOUCH)) {
                    resetJoueur();
                    niveau = 0;
                    initEntites();
                    etat = ETAT_JEU;
                }
                break;

            case ETAT_JEU:
                gererEntrees();
                majEntites();
                majParticules();
                majCamera();
                verifierProgression();
                break;

            case ETAT_TRANSITION:
                chronoTransition -= 1.0f/60.0f;
                majCamera();
                if (chronoTransition <= 0) etat = ETAT_JEU;
                break;

            case ETAT_GAMEOVER:
            case ETAT_VICTOIRE:
                if (kDown & (KEY_A | KEY_TOUCH)) {
                    resetJoueur();
                    niveau = 0;
                    initEntites();
                    etat = ETAT_JEU;
                }
                break;
        }

        // ---------------- RENDU -----------------
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // ECRAN DU HAUT : le jeu
        C2D_TargetClear(topTarget, clrFond[niveau]);
        C2D_SceneBegin(topTarget);

        if (etat == ETAT_TITRE) {
            char buf[64];
            snprintf(buf, sizeof(buf), "ABYSSE");
            C2D_Text titre;
            C2D_TextBufClear(textBuf);
            C2D_TextParse(&titre, textBuf, buf);
            C2D_TextOptimize(&titre);
            C2D_DrawText(&titre, C2D_AlignCenter | C2D_WithColor, 200.0f, 90.0f, 0.0f, 1.6f, 1.6f, C2D_Color32f(1,1,1,1));

            C2D_Text sous;
            C2D_TextParse(&sous, textBuf, "Mange. Grandis. Domine.");
            C2D_TextOptimize(&sous);
            C2D_DrawText(&sous, C2D_AlignCenter | C2D_WithColor, 200.0f, 130.0f, 0.0f, 0.7f, 0.7f, C2D_Color32f(1,1,1,0.85f));

            dessinerPoisson(200, 170, 30, 1.0f, 0.45f, 0.05f, 1, anim_nage);
            anim_nage += 0.1f;
        }
        else if (etat == ETAT_JEU || etat == ETAT_TRANSITION) {
            dessinerFondBulles();
            dessinerEntites();
            dessinerJoueur();
            dessinerParticules();

            if (etat == ETAT_TRANSITION) {
                u32 voile = C2D_Color32f(0,0,0, 0.5f * (chronoTransition/2.2f > 1.0f ? 1.0f : chronoTransition/2.2f));
                C2D_DrawRectSolid(0, 0, 0.0f, SCREEN_W, SCREEN_H, voile);

                char buf[64];
                snprintf(buf, sizeof(buf), "%s", nomForme(formeJoueur));
                C2D_TextBufClear(textBuf);
                C2D_Text txt;
                C2D_TextParse(&txt, textBuf, buf);
                C2D_TextOptimize(&txt);
                C2D_DrawText(&txt, C2D_AlignCenter | C2D_WithColor, 200.0f, 100.0f, 0.0f, 1.0f, 1.0f, C2D_Color32f(1,1,1,1));

                char buf2[64];
                snprintf(buf2, sizeof(buf2), "Bienvenue dans : %s", nomNiveau(niveau));
                C2D_Text txt2;
                C2D_TextParse(&txt2, textBuf, buf2);
                C2D_TextOptimize(&txt2);
                C2D_DrawText(&txt2, C2D_AlignCenter | C2D_WithColor, 200.0f, 130.0f, 0.0f, 0.7f, 0.7f, C2D_Color32f(1,1,0.6f,1));
            }
        }
        else if (etat == ETAT_GAMEOVER) {
            C2D_Text txt;
            C2D_TextBufClear(textBuf);
            C2D_TextParse(&txt, textBuf, "PERDU");
            C2D_TextOptimize(&txt);
            C2D_DrawText(&txt, C2D_AlignCenter | C2D_WithColor, 200.0f, 100.0f, 0.0f, 1.4f, 1.4f, C2D_Color32f(1,0.3f,0.3f,1));
        }
        else if (etat == ETAT_VICTOIRE) {
            C2D_Text txt;
            C2D_TextBufClear(textBuf);
            C2D_TextParse(&txt, textBuf, "TU DOMINES LES ABYSSES !");
            C2D_TextOptimize(&txt);
            C2D_DrawText(&txt, C2D_AlignCenter | C2D_WithColor, 200.0f, 100.0f, 0.0f, 1.1f, 1.1f, C2D_Color32f(1,0.9f,0.3f,1));
        }

        // ECRAN DU BAS : infos / HUD
        C2D_TargetClear(botTarget, clrFondBas[niveau]);
        C2D_SceneBegin(botTarget);

        {
            char buf[64];
            C2D_TextBufClear(textBuf);

            snprintf(buf, sizeof(buf), "Zone : %s", nomNiveau(niveau));
            C2D_Text t1;
            C2D_TextParse(&t1, textBuf, buf);
            C2D_TextOptimize(&t1);
            C2D_DrawText(&t1, C2D_WithColor, 12.0f, 14.0f, 0.0f, 0.6f, 0.6f, C2D_Color32f(1,1,1,1));

            snprintf(buf, sizeof(buf), "Forme : %s", nomForme(formeJoueur));
            C2D_Text t2;
            C2D_TextParse(&t2, textBuf, buf);
            C2D_TextOptimize(&t2);
            C2D_DrawText(&t2, C2D_WithColor, 12.0f, 34.0f, 0.0f, 0.6f, 0.6f, C2D_Color32f(1,1,1,1));

            snprintf(buf, sizeof(buf), "Taille : %d", (int)joueurTaille);
            C2D_Text t3;
            C2D_TextParse(&t3, textBuf, buf);
            C2D_TextOptimize(&t3);
            C2D_DrawText(&t3, C2D_WithColor, 12.0f, 54.0f, 0.0f, 0.6f, 0.6f, C2D_Color32f(1,1,1,1));

            snprintf(buf, sizeof(buf), "Score : %d", score);
            C2D_Text t4;
            C2D_TextParse(&t4, textBuf, buf);
            C2D_TextOptimize(&t4);
            C2D_DrawText(&t4, C2D_WithColor, 12.0f, 74.0f, 0.0f, 0.6f, 0.6f, C2D_Color32f(1,1,0.5f,1));

            C2D_Text t5;
            C2D_TextParse(&t5, textBuf,
                etat == ETAT_TITRE ? "Appuie sur A pour commencer" :
                (etat == ETAT_GAMEOVER || etat == ETAT_VICTOIRE) ? "Appuie sur A pour rejouer" :
                "Stick / croix : se deplacer");
            C2D_TextOptimize(&t5);
            C2D_DrawText(&t5, C2D_WithColor, 12.0f, 200.0f, 0.0f, 0.55f, 0.55f, C2D_Color32f(0.8f,0.95f,1.0f,1));
        }

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
