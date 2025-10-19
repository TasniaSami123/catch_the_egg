#include <GL/glut.h>

#include <cmath>

#include <vector>

#include <string>

#include <cstdlib>

#include <ctime>

#include <algorithm>


struct Vec2 { float x,y; };

float frand(float a, float b){ return a + (b-a)*(rand()/(float)RAND_MAX); }


enum class ObjType { NormalEgg, BlueEgg, GoldenEgg, Poop, PerkSize, PerkSlow, PerkTime };

enum class Screen  { Menu, Help, Playing, Paused, GameOver };


struct Falling {

    ObjType type;

    Vec2 pos;

    float vy;

    float radius;

    float rot=0.0f, rotSpd=0.0f;

    bool  active=true;

};


struct Basket {

    float x=0.0f, y=-0.8f;

    float halfW=0.16f, h=0.09f;

};


struct Chicken {

    float x=0.0f, y=0.70f;

    float vx=0.45f;

    float bob=0.0f;

};


struct Particle {

    Vec2 p, v;

    float life, maxLife;

    float size;

    float r,g,b,a;

};


struct FloatText {

    Vec2 p;

    std::string s;

    float life=1.0f;

    float vy=0.35f;

    float r=0,g=0,b=0;

};


static int   winW=900, winH=600;

static float worldL=-1, worldR=1, worldB=-1, worldT=1;


static Screen screenState = Screen::Menu;

static int menuIndex=0;


static Basket basket;

static Chicken chicken;

static std::vector<Falling> objs;

static std::vector<Particle> parts;

static std::vector<FloatText> floatTexts;


static int score=0, highScore=0;

static int timeLeft=60;


static bool slowFall=false;  static float slowFallTimer=0;

static bool bigBasket=false; static float bigBasketTimer=0;


static float spawnTimer=0, spawnEvery=0.65f;

static int   lastTick=0;


static float shake=0.0f;


// --- Mouse / Button support

struct Button {

    float x1, y1, x2, y2;  // in world‑coords

    std::string label;

    void (*action)();

};


static std::vector<Button> menuButtons;


// Utility to map window coordinate to world coordinate

Vec2 windowToWorld(int mx, int my) {

    float wx = worldL + (mx / (float)winW) * (worldR - worldL);

    float wy = worldB + ((winH - my) / (float)winH) * (worldT - worldB);

    return {wx, wy};

}


void startGame();

void helpScreen();

void exitGame();


bool isOver(const Button& b, const Vec2& wpos) {

    return (wpos.x >= b.x1 && wpos.x <= b.x2 && wpos.y >= b.y1 && wpos.y <= b.y2);

}


void mouseClick(int button, int state, int x, int y) {

    if (state != GLUT_DOWN) return;

    Vec2 wpos = windowToWorld(x,y);

    if (screenState == Screen::Menu) {

        for (auto &b : menuButtons) {

            if (isOver(b, wpos)) {

                b.action();

                return;

            }

        }

    } else if (screenState == Screen::GameOver) {

        // Define clickable regions for game‑over options.

        // We’ll approximate via world coords:

        // "Enter: Menu" – we treat clicking the left half

        // "S: Restart" – clicking the right half

        // You could add explicit buttons just like menuButtons if you prefer

        if (wpos.x < 0) {

            // Return to menu

            screenState = Screen::Menu;

        } else {

            // Restart

            startGame();

        }

    } else if (screenState == Screen::Help) {

        // On help screen, clicking anywhere returns to menu

        screenState = Screen::Menu;

    }

}


// --- Setup buttons for menu

void setupMenuButtons() {

    // Let's define four buttons arranged vertically

    // Using world coordinates roughly matching your drawMenu layout

    float bw = 0.36f, bh = 0.07f;

    float cx = 0.0f;

    float baseY = 0.15f;

    menuButtons.clear();

    // “Start”

    menuButtons.push_back({ cx - bw, baseY - bh, cx + bw, baseY + bh, "Start", &startGame });

    // “Resume” (only meaningful if returning from pause or something)

    menuButtons.push_back({ cx - bw, baseY - 0.12f - bh, cx + bw, baseY - 0.12f + bh, "Resume", [](){

        screenState = Screen::Playing;

    }});

    // “Help”

    menuButtons.push_back({ cx - bw, baseY - 0.24f - bh, cx + bw, baseY - 0.24f + bh, "Help", &helpScreen });

    // “Exit”

    menuButtons.push_back({ cx - bw, baseY - 0.36f - bh, cx + bw, baseY - 0.36f + bh, "Exit", &exitGame });

}


// Action functions

void startGame(){

    // Reset game

    objs.clear(); parts.clear(); floatTexts.clear();

    score=0; timeLeft=60; spawnTimer=0; spawnEvery=0.65f;

    slowFall=false; slowFallTimer=0;

    bigBasket=false; bigBasketTimer=0; basket=Basket();

    chicken=Chicken(); shake=0.0f;

    lastTick = glutGet(GLUT_ELAPSED_TIME);

    screenState = Screen::Playing;

}


void helpScreen(){

    screenState = Screen::Help;

}


void exitGame(){

    exit(0);

}


// --- Rendering / logic functions (mostly unchanged) …


void ortho(){

    glMatrixMode(GL_PROJECTION); glLoadIdentity();

    gluOrtho2D(worldL, worldR, worldB, worldT);

    glMatrixMode(GL_MODELVIEW);

}


void enableSmooth(){

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glEnable(GL_POINT_SMOOTH); glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

}


void drawRect(float x,float y,float w,float h){

    glBegin(GL_QUADS);

    glVertex2f(x-w,y-h); glVertex2f(x+w,y-h);

    glVertex2f(x+w,y+h); glVertex2f(x-w,y+h);

    glEnd();

}

void drawCircle(float x,float y,float r,int seg=48){

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(x,y);

    for(int i=0;i<=seg;i++){

        float th=2.0f*3.1415926f*i/seg;

        glVertex2f(x+cosf(th)*r,y+sinf(th)*r);

    }

    glEnd();

}

void drawRing(float x,float y,float r,float t,int seg=64){

    glBegin(GL_TRIANGLE_STRIP);

    for(int i=0;i<=seg;i++){

        float th=2.0f*3.1415926f*i/seg;

        float cx=cosf(th), sy=sinf(th);

        glVertex2f(x+(r-t)*cx, y+(r-t)*sy);

        glVertex2f(x+r*cx,     y+r*sy);

    }

    glEnd();

}

void drawText(float x,float y,const std::string& s, void* font=GLUT_BITMAP_HELVETICA_18){

    glRasterPos2f(x,y);

    for(char c: s) glutBitmapCharacter(font, c);

}


void addParticles(Vec2 p, int n, float r,float g,float b){

    for(int i=0;i<n;i++){

        Particle q;

        q.p=p;

        float ang=frand(0,2*3.14159f);

        float sp=frand(0.6f,1.6f);

        q.v={cosf(ang)*sp, sinf(ang)*sp};

        q.life=q.maxLife=frand(0.35f,0.75f);

        q.size=frand(0.008f,0.02f);

        q.r=r; q.g=g; q.b=b; q.a=1.0f;

        parts.push_back(q);

    }

}

void addFloatText(Vec2 p, const std::string& s, float r,float g,float b){

    FloatText ft; ft.p=p; ft.s=s; ft.r=r; ft.g=g; ft.b=b; ft.life=1.1f;

    floatTexts.push_back(ft);

}


Falling makeObj(ObjType t){

    Falling f; f.type=t;

    f.pos={ chicken.x + frand(-0.05f,0.05f), chicken.y-0.06f };

    f.vy = - frand(0.45f,0.65f);

    f.radius = (t==ObjType::GoldenEgg? 0.045f : t==ObjType::Poop? 0.032f : 0.038f);

    f.rotSpd = frand(-120,120);

    return f;

}


void applyPerk(ObjType t){

    if(t==ObjType::PerkSize){

        bigBasket=true; bigBasketTimer=8; basket.halfW=0.24f;

        addFloatText({basket.x,basket.y+0.12f},"BIG!",0.2f,0.8f,0.3f);

    }

    if(t==ObjType::PerkSlow){

        slowFall=true; slowFallTimer=8;

        addFloatText({basket.x,basket.y+0.12f},"SLOW",0.8f,0.2f,0.8f);

    }

    if(t==ObjType::PerkTime){

        timeLeft = std::min(120, timeLeft+10);

        addFloatText({0.0f,0.9f},"+10s",0.2f,0.7f,0.9f);

    }

}


bool aabbCircleCollide(const Basket& b, const Falling& f){

    float cx=std::max(b.x-b.halfW, std::min(f.pos.x, b.x+b.halfW));

    float cy=std::max(b.y-b.h,     std::min(f.pos.y, b.y+b.h));

    float dx=f.pos.x-cx, dy=f.pos.y-cy;

    return dx*dx+dy*dy <= f.radius*f.radius;

}


void drawGradientBG(){

    glBegin(GL_QUADS);

    glColor3f(0.62f,0.85f,1.0f); glVertex2f(worldL, 0.15f);

    glColor3f(0.62f,0.85f,1.0f); glVertex2f(worldR, 0.15f);

    glColor3f(0.87f,0.95f,1.0f); glVertex2f(worldR, worldT);

    glColor3f(0.87f,0.95f,1.0f); glVertex2f(worldL, worldT);

    glEnd();


    glColor4f(1.0f,0.93f,0.45f,0.9f); drawCircle(-0.78f,0.82f,0.10f,48);


    glColor3f(0.65f,0.85f,0.65f);

    drawRect(-0.2f,0.22f, 1.2f,0.12f);

    glColor3f(0.55f,0.78f,0.55f);

    drawRect(0.15f,0.12f, 1.1f,0.10f);


    glColor3f(0.28f,0.72f,0.35f); drawRect(0,-0.94f,1.0f,0.14f);


    glLineWidth(2);

    glColor3f(0.18f,0.55f,0.25f);

    glBegin(GL_LINES);

    for(int i=0;i<50;i++){

        float x=worldL+0.04f*i+fmodf(i*0.03f,0.02f);

        glVertex2f(x,-0.92f); glVertex2f(x+0.01f,-0.88f);

    }

    glEnd();


    glColor3f(1,1,1); drawRect(0,0.93f,1.0f,0.07f);

}
