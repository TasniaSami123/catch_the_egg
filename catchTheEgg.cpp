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
void eggShape(const Falling& o){

    if(o.type==ObjType::GoldenEgg){ glColor3f(1.0f,0.84f,0.0f); }

    else if(o.type==ObjType::BlueEgg){ glColor3f(0.45f,0.65f,1.0f); }

    else if(o.type==ObjType::NormalEgg){ glColor3f(1.0f,1.0f,0.94f); }


    drawCircle(0,0,o.radius,40);

    glColor4f(1,1,1,0.45f);

    drawCircle(-o.radius*0.35f,o.radius*0.25f,o.radius*0.35f,28);


    if(o.type==ObjType::GoldenEgg){

        glColor4f(1.0f,0.9f,0.2f,0.25f);

        drawRing(0,0,o.radius*1.35f, o.radius*0.15f, 56);

    }

}

void poopShape(const Falling& o){

    glColor3f(0.45f,0.25f,0.06f);

    drawCircle(0,0,o.radius*0.9f,20);

    drawCircle(0, o.radius*0.45f,o.radius*0.7f,20);

    drawCircle(0, o.radius*0.9f, o.radius*0.45f,20);

}

void perkShape(const Falling& o){

    if(o.type==ObjType::PerkSize) glColor3f(0.2f,0.85f,0.35f);

    if(o.type==ObjType::PerkSlow) glColor3f(0.85f,0.3f,0.85f);

    if(o.type==ObjType::PerkTime) glColor3f(0.2f,0.7f,0.9f);

    drawRect(0,0,o.radius*1.15f,o.radius*1.15f);

    glColor4f(1,1,1,0.5f);

    drawRect(-o.radius*0.12f,o.radius*0.12f,o.radius*0.35f,o.radius*0.08f);

}


void drawChicken(){

    glPushMatrix();

    glTranslatef(chicken.x, chicken.y+chicken.bob, 0);


    glColor3f(1.0f,0.76f,0.35f); drawCircle(0,0,0.065f,36);

    glColor3f(1.0f,0.86f,0.55f); drawCircle(-0.015f,-0.01f,0.045f,28);

    glColor3f(1.0f,0.76f,0.35f); drawCircle(0.055f,0.05f,0.037f,28);


    glColor3f(0.9f,0.2f,0.2f);

    drawCircle(0.04f,0.08f,0.012f,16);

    drawCircle(0.055f,0.09f,0.012f,16);

    drawCircle(0.07f,0.08f,0.012f,16);


    glColor3f(1.0f,0.45f,0.1f);

    glBegin(GL_TRIANGLES);

    glVertex2f(0.09f,0.05f); glVertex2f(0.115f,0.055f); glVertex2f(0.09f,0.065f);

    glEnd();


    glColor3f(0,0,0); drawCircle(0.064f,0.058f,0.006f,14);


    glPopMatrix();

}


void drawBasket(){

    glPushMatrix();

    glTranslatef(basket.x, basket.y, 0);

    glColor4f(0,0,0,0.15f); drawRect(0,-0.02f,basket.halfW*0.95f,0.02f);

    glColor3f(0.92f,0.72f,0.45f); drawRect(0, 0.0f, basket.halfW, basket.h*0.85f);


    glColor3f(0.8f,0.6f,0.35f); glLineWidth(2);

    glBegin(GL_LINES);

    for(float y=-basket.h*0.7f; y<=basket.h*0.7f; y+=basket.h*0.28f){

        glVertex2f(-basket.halfW,y); glVertex2f(basket.halfW,y);

    }

    glEnd();


    glColor3f(0.55f,0.35f,0.15f);

    glBegin(GL_QUADS);

    glVertex2f(-basket.halfW, basket.h*0.85f);

    glVertex2f( basket.halfW, basket.h*0.85f);

    glVertex2f( basket.halfW*0.9f, basket.h*1.05f);

    glVertex2f(-basket.halfW*0.9f, basket.h*1.05f);

    glEnd();


    glPopMatrix();

}


void drawObj(const Falling& o){

    glPushMatrix();

    glTranslatef(o.pos.x, o.pos.y, 0);

    glRotatef(o.rot, 0,0,1);

    if(o.type==ObjType::Poop) poopShape(o);

    else if(o.type==ObjType::PerkSize || o.type==ObjType::PerkSlow || o.type==ObjType::PerkTime) perkShape(o);

    else eggShape(o);

    glPopMatrix();

}


void drawParticles(){

    for(const auto& p: parts){

        glColor4f(p.r, p.g, p.b, p.a);

        drawCircle(p.p.x, p.p.y, p.size, 10);

    }

}

void drawFloatTexts(){

    for(const auto& ft: floatTexts){

        glColor3f(ft.r, ft.g, ft.b);

        drawText(ft.p.x-0.02f, ft.p.y, ft.s);

    }

}


void drawHUD(){

    glColor3f(0,0,0);

    drawText(-0.95f,0.92f,"Score: "+std::to_string(score));

    drawText(-0.22f,0.92f,"Time: "+std::to_string(timeLeft));

    if(bigBasket) drawText(0.32f,0.92f,"BIG ("+std::to_string((int)ceilf(bigBasketTimer))+"s)");

    if(slowFall)  drawText(0.58f,0.92f,"SLOW ("+std::to_string((int)ceilf(slowFallTimer))+"s)");

}


void drawMenu(){

    glColor3f(0.1f,0.1f,0.1f); drawText(-0.36f,0.72f,"CATCH THE EGGS");


    drawGradientBG();

    drawChicken(); drawBasket();


    for (auto &b : menuButtons){

        bool isSel = false; // we have no hover logic currently

        glColor4f(0,0,0,0.2f); drawRect((b.x1+b.x2)/2, (b.y1+b.y2)/2 - 0.008f, (b.x2-b.x1)/2, (b.y2-b.y1)/2);

        glColor3f(isSel?0.25f:0.95f, isSel?0.85f:0.95f, isSel?0.35f:0.95f);

        drawRect((b.x1+b.x2)/2, (b.y1+b.y2)/2, (b.x2-b.x1)/2, (b.y2-b.y1)/2);

        glColor3f(0,0,0);

        drawText((b.x1+b.x2)/2 - 0.06f, (b.y1+b.y2)/2 - 0.01f, b.label);

    }


    glColor3f(0,0,0); drawText(-0.2f,-0.42f,"High Score: "+std::to_string(highScore));

}


void updateGame(float dt){

    chicken.x += chicken.vx*dt;

    if(chicken.x>0.82f){ chicken.x=0.82f; chicken.vx*=-1; }

    if(chicken.x<-0.82f){ chicken.x=-0.82f; chicken.vx*=-1; }

    chicken.bob = 0.01f * sinf(glutGet(GLUT_ELAPSED_TIME)*0.008f);


    spawnTimer+=dt;

    if(spawnTimer>=spawnEvery){

        objs.push_back(makeObj(static_cast<ObjType>(rand()%7)));

        spawnTimer=0;

        spawnEvery=std::max(0.35f, spawnEvery-0.0025f);

    }


    if(bigBasket){

        bigBasketTimer-=dt;

        if(bigBasketTimer<=0){

            bigBasket=false;

            basket.halfW=0.16f;

        }

    }

    if(slowFall){

        slowFallTimer-=dt;

        if(slowFallTimer<=0){

            slowFall=false;

        }

    }


    for(auto &o: objs){

        if(!o.active) continue;

        float mult = slowFall?0.55f:1.0f;

        o.pos.y += o.vy*mult*dt;

        o.rot   += o.rotSpd*dt;


        if(aabbCircleCollide(basket,o)){

            o.active=false;

            switch(o.type){

                case ObjType::NormalEgg: score+=1;   addParticles(o.pos,12,1,1,0.9f); addFloatText(o.pos,"+1",0,0.6f,0); break;

                case ObjType::BlueEgg:   score+=5;   addParticles(o.pos,16,0.4f,0.6f,1); addFloatText(o.pos,"+5",0.1f,0.45f,1); break;

                case ObjType::GoldenEgg: score+=10;  addParticles(o.pos,20,1.0f,0.84f,0.0f); addFloatText(o.pos,"+10",0.95f,0.7f,0); break;

                case ObjType::Poop:      score=std::max(0,score-10); addParticles(o.pos,18,0.45f,0.25f,0.05f); addFloatText(o.pos,"-10",0.7f,0.2f,0.1f); shake=0.12f; break;

                case ObjType::PerkSize:

                case ObjType::PerkSlow:

                case ObjType::PerkTime:  applyPerk(o.type); addParticles(o.pos,14,0.8f,0.8f,0.8f); break;

            }

        }

        if(o.pos.y < worldB - 0.25f) o.active=false;

    }

    objs.erase(std::remove_if(objs.begin(),objs.end(),[](const Falling& f){return !f.active;}), objs.end());


    for(auto &p: parts){

        p.p.x += p.v.x*dt; p.p.y += p.v.y*dt;

        p.v.y -= 1.6f*dt;

        p.life -= dt;

        p.a = std::max(0.0f, p.life/p.maxLife);

    }

    parts.erase(std::remove_if(parts.begin(),parts.end(),[](const Particle& p){return p.life<=0;}), parts.end());


    for(auto &ft: floatTexts){

        ft.p.y += ft.vy*dt;

        ft.life -= dt;

    }

    floatTexts.erase(std::remove_if(floatTexts.begin(),floatTexts.end(),[](const FloatText& t){return t.life<=0;}), floatTexts.end());


    if(shake>0){ shake = std::max(0.0f, shake - dt*0.7f); }


    static float accu=0;

    accu += dt;

    if(accu >= 1.0f){

        timeLeft -= 1;

        accu = 0;

        if(timeLeft <= 0){

            screenState = Screen::GameOver;

            highScore = std::max(highScore, score);

        }

    }

}


void display(){

    glClearColor(0.96f,0.96f,0.98f,1); glClear(GL_COLOR_BUFFER_BIT);

    glLoadIdentity();


    if(shake>0){

        float ox = frand(-shake,shake)*0.015f;

        float oy = frand(-shake,shake)*0.015f;

        glTranslatef(ox,oy,0);

    }


    if(screenState==Screen::Menu){

        drawMenu();

    } else if(screenState==Screen::Help){

        drawGradientBG();

        glColor3f(0,0,0);

        drawText(-0.95f,0.8f,"Help:");

        drawText(-0.95f,0.68f,"- Move: Left/Right or A/D or Mouse");

        drawText(-0.95f,0.58f,"- Eggs: +1, +5, +10; Poop: -10");

        drawText(-0.95f,0.48f,"- Perks: Green=BIG, Purple=SLOW, Cyan=+10s");

        drawText(-0.95f,0.38f,"- P: Pause/Resume, Enter: Select, Esc: Back");

        drawText(-0.95f,-0.90f,"Click anywhere to return to Menu.");

    } else if(screenState==Screen::Playing || screenState==Screen::Paused){

        drawGradientBG();

        glColor3f(0.62f,0.45f,0.18f); drawRect(0,0.65f,0.92f,0.018f);

        drawChicken();

        for(const auto& o: objs) drawObj(o);

        drawBasket();

        drawParticles();

        drawHUD();

        drawFloatTexts();


        if(screenState==Screen::Paused){

            glColor4f(0,0,0,0.6f); drawRect(0,0,0.7f,0.25f);

            glColor3f(1,1,1); drawText(-0.11f,0.02f,"PAUSED");

            drawText(-0.32f,-0.06f,"Press P to Resume, Esc for Menu");

        }

    } else if(screenState==Screen::GameOver){

        drawGradientBG();

        glColor4f(0,0,0,0.7f); drawRect(0,0,0.82f,0.35f);

        glColor3f(1,1,1);

        drawText(-0.16f,0.10f,"GAME OVER");

        drawText(-0.32f,-0.02f,"Score: "+std::to_string(score)+"   Best: "+std::to_string(highScore));

        drawText(-0.44f,-0.16f,"Click left = Menu     Click right = Restart");

    }


    glutSwapBuffers();

}


void reshape(int w,int h){ winW=w; winH=h; glViewport(0,0,w,h); ortho(); }


void keyboard(unsigned char key,int,int){

    if(key==27){ // Esc

        if(screenState==Screen::Playing || screenState==Screen::Paused) screenState=Screen::Menu;

        else exit(0);

    } else if(key=='p'||key=='P'){

        if(screenState==Screen::Playing) screenState=Screen::Paused;

        else if(screenState==Screen::Paused) screenState=Screen::Playing;

    } else if(key=='s'||key=='S'){

        if(screenState==Screen::GameOver){ startGame(); }

    } else if(key==13){ // Enter

        if(screenState==Screen::Menu){

            // choose Start by default

            startGame();

        } else if(screenState==Screen::Help || screenState==Screen::GameOver){

            screenState=Screen::Menu;

        }

    }

}

void special(int key,int,int){

    if(screenState==Screen::Menu){

        if(key==GLUT_KEY_UP) menuIndex=(menuIndex+3)%4;

        if(key==GLUT_KEY_DOWN) menuIndex=(menuIndex+1)%4;

    } else if(screenState==Screen::Playing){

        if(key==GLUT_KEY_LEFT)  basket.x -= 0.08f;

        if(key==GLUT_KEY_RIGHT) basket.x += 0.08f;

        basket.x = std::max(worldL+basket.halfW, std::min(worldR-basket.halfW, basket.x));

    }

}

void passiveMotion(int mx,int){

    if(screenState==Screen::Playing){

        float norm = mx/(float)winW;

        float wx = worldL + norm*(worldR-worldL);

        basket.x = std::max(worldL+basket.halfW, std::min(worldR-basket.halfW, wx));

    }

}


void timerFunc(int){

    int now = glutGet(GLUT_ELAPSED_TIME);

    float dt = (now - lastTick)/1000.0f;

    lastTick = now;

    dt = std::min(dt, 0.033f);


    if(screenState==Screen::Playing){

        updateGame(dt);

    }

    glutPostRedisplay();

    glutTimerFunc(16, timerFunc, 0);

}


int main(int argc,char** argv){

    srand((unsigned)time(nullptr));

    glutInit(&argc,argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);

    glutInitWindowSize(winW,winH);

    glutCreateWindow("Catch The Eggs - Pretty Edition");

    ortho(); enableSmooth();


    setupMenuButtons();


    glutDisplayFunc(display);

    glutReshapeFunc(reshape);

    glutKeyboardFunc(keyboard);

    glutSpecialFunc(special);

    glutPassiveMotionFunc(passiveMotion);

    glutMouseFunc(mouseClick);


    lastTick = glutGet(GLUT_ELAPSED_TIME);

    glutTimerFunc(16, timerFunc, 0);


    glutMainLoop();

    return 0;

}
