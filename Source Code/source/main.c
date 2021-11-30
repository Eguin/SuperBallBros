/*===========================================
    GRRLIB goes up to 640 on the x axis and 479 on the y axis
============================================*/
#include <grrlib.h>

#include <stdlib.h>
#include <math.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include <asndlib.h>
#include <mp3player.h>

#include "gfx/PlayerFace.h"
#include "gfx/BasicBlackGun.h"
#include "gfx/PressA.h"
#include "gfx/PressB.h"
#include "gfx/Player1Wins.h"
#include "gfx/Player2Wins.h"
#include "gfx/Player3Wins.h"
#include "gfx/Player4Wins.h"
#include "InGameSong_mp3.h"
#include "OpeningSong_mp3.h"
#include "WinSong_mp3.h"

// RGBA Colors
#define GRRLIB_BLACK   0x000000FF
#define GRRLIB_MAROON  0x800000FF
#define GRRLIB_GREEN   0x008000FF
#define GRRLIB_OLIVE   0x808000FF
#define GRRLIB_NAVY    0x000080FF
#define GRRLIB_PURPLE  0x800080FF
#define GRRLIB_TEAL    0x008080FF
#define GRRLIB_GRAY    0x808080FF
#define GRRLIB_SILVER  0xC0C0C0FF
#define GRRLIB_RED     0xFF0000FF
#define GRRLIB_LIME    0x00FF00FF
#define GRRLIB_YELLOW  0xFFFF00FF
#define GRRLIB_BLUE    0x0000FFFF
#define GRRLIB_FUCHSIA 0xFF00FFFF
#define GRRLIB_AQUA    0x00FFFFFF
#define GRRLIB_WHITE   0xFFFFFFFF
#define boxesSize 4
const u32 playerColors[4]={GRRLIB_RED,GRRLIB_BLUE,GRRLIB_GREEN,GRRLIB_YELLOW};
const int xend = 640;
const int yend = 479;
const int playerSize = 26;
const int gunSize=6;
double deltaTime;
uint64_t deltaTimeStart;
int scene=1;
int winner;

float degToRad(float d){
    return 0.0174532925*d;
}

float radToDeg(float d){
    return (180/3.1415926)*d;
}

struct gun{
    GRRLIB_texImg *gunIMG;
    float damage;
    float knockback;
    float reloadTime;
    float spread;
    float range;
    int xcenter;
    int ycenter;
    int index;
};

struct gun gunsInGame[5];
int amountOfGunsInGame=0;

struct character{
    int playerNumber;
    GRRLIB_texImg *playerIMG;
    int xpos;
    int ypos;
    int xcenter;
    int ycenter;
    int jump;
    bool isJumping;
    int timeInAir;
    u16 down;
    u16 held;
    bool gcController;
    int controllerNumber;
    float damage;
    float xvel;
    float yvel;
    float xvelmov;
    float spin;
    float spinvel;
    float spinvelmov;
    bool hasGun;
    struct gun playerGun;
    float cStickDegree;
    bool inGame;
    int lives;
};
struct character player[4];
int playersInGame=0;


struct box{
    int x;
    int y;
    int width;
    int height;
};

void boxInit(struct box *b, int x, int y, int w, int h){
    b->x=x;
    b->y=y;
    b->width=w;
    b->height=h;
}

struct box boxes[boxesSize];

float distanceBetween(int x1, int y1, int x2, int y2){
    return sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

float angleBetween(int x1, int y1, int x2, int y2){
    float ydiff=y1-y2;
    float xdiff=x2-x1;
    return atan2(ydiff, xdiff);
}

bool touchingBox(struct box b, struct character c){
    if(abs(c.ycenter-b.y)<=26 && c.xcenter+10>b.x && c.xcenter-10<b.x+b.width){
        return true;
    }
    /*if(abs(c.xcenter-x)<=26){
        return true;
    }*/
    return false;
}

void gun_INIT(struct gun *g, const u8 *img, float d, float k, float rt, float s, float r){
    g->gunIMG=GRRLIB_LoadTexture(img);
    g->damage=d;
    g->knockback=k;
    g->reloadTime=rt;
    g->spread=s;
    g->range=r;
}

void player_INIT(struct character *c, const u8 *img, bool gc, int cnum, int pnum, int l){
    playersInGame++;
    c->playerIMG =  GRRLIB_LoadTexture(img);
    c->xcenter=320;
    c->ycenter=10;
    c->jump=0;
    c->isJumping=false;
    c->timeInAir=0;
    c->gcController=gc;
    c->controllerNumber=cnum;
    c->playerNumber=pnum;
    c->damage=1;
    c->xvel=sin(degToRad(180));
    c->yvel=0;
    c->xvelmov=0;
    c->spin=0;
    c->spinvel=0;
    c->spinvelmov=0;
    c->cStickDegree=0;
    c->hasGun=false;
    c->inGame=true;
    c->lives=l;
}

void equipGun(struct character *c, struct gun g){
    c->hasGun=true;
    c->playerGun=g;
    c->cStickDegree=0;
}

void unEquipGun(struct character *c){
    c->hasGun=false;
}

void pickupGun(struct character *c){
    for(int i=0;i<amountOfGunsInGame;i++){
        if(distanceBetween(c->xcenter,c->ycenter,gunsInGame[i].xcenter,gunsInGame[i].ycenter)<=(playerSize+gunSize)*1.5){
            equipGun(c,gunsInGame[i]);
            for(int j=i+1;j<amountOfGunsInGame;j++){
                gunsInGame[j-1]=gunsInGame[j];
            }
            amountOfGunsInGame--;
            break;
        }
    }
}

void shoot(struct character c){
    if(c.playerNumber!=1 && (player[0]).inGame && distanceBetween(c.xcenter,c.ycenter, (player[0]).xcenter, (player[0]).ycenter)<=c.playerGun.range && abs(radToDeg(c.cStickDegree)-radToDeg(angleBetween(c.xcenter,c.ycenter, (player[0]).xcenter, (player[0]).ycenter)))<=c.playerGun.spread){
        (player[0]).xvel+=cos(c.cStickDegree)*c.playerGun.knockback*(player[0]).damage;
        (player[0]).yvel-=sin(c.cStickDegree)*c.playerGun.knockback*(player[0]).damage;
        (player[0]).damage+=c.playerGun.damage;
    }
    if(c.playerNumber!=2 && (player[1]).inGame && distanceBetween(c.xcenter,c.ycenter, (player[1]).xcenter, (player[1]).ycenter)<=c.playerGun.range && abs(radToDeg(c.cStickDegree)-radToDeg(angleBetween(c.xcenter,c.ycenter, (player[1]).xcenter, (player[1]).ycenter)))<=c.playerGun.spread){
        (player[1]).xvel+=cos(c.cStickDegree)*c.playerGun.knockback*(player[1]).damage;
        (player[1]).yvel-=sin(c.cStickDegree)*c.playerGun.knockback*(player[1]).damage;
        (player[1]).damage+=c.playerGun.damage;
    }
    if(c.playerNumber!=3 && (player[2]).inGame && distanceBetween(c.xcenter,c.ycenter, (player[2]).xcenter, (player[2]).ycenter)<=c.playerGun.range && abs(radToDeg(c.cStickDegree)-radToDeg(angleBetween(c.xcenter,c.ycenter, (player[2]).xcenter, (player[2]).ycenter)))<=c.playerGun.spread){
        (player[2]).xvel+=cos(c.cStickDegree)*c.playerGun.knockback*(player[2]).damage;
        (player[2]).yvel-=sin(c.cStickDegree)*c.playerGun.knockback*(player[2]).damage;
        (player[2]).damage+=c.playerGun.damage;
    }
    if(c.playerNumber!=4 && (player[3]).inGame && distanceBetween(c.xcenter,c.ycenter, (player[3]).xcenter, (player[3]).ycenter)<=c.playerGun.range && abs(radToDeg(c.cStickDegree)-radToDeg(angleBetween(c.xcenter,c.ycenter, (player[3]).xcenter, (player[3]).ycenter)))<=c.playerGun.spread){
        (player[3]).xvel+=cos(c.cStickDegree)*c.playerGun.knockback*(player[3]).damage;
        (player[3]).yvel-=sin(c.cStickDegree)*c.playerGun.knockback*(player[3]).damage;
        (player[3]).damage+=c.playerGun.damage;
    }
}

void endGame(){
    for(int i=0;i<4;i++){
        if(player[i].inGame){
            winner=i;
        }
        player[i].inGame=false;
    }
    GRRLIB_SetBackgroundColour(0x80, 0x80, 0x80, 0xFF);
    MP3Player_Stop();
    MP3Player_PlayBuffer(WinSong_mp3, WinSong_mp3_size, NULL);
    playersInGame=0;
    scene=3;
}

void killPlayer(struct character *c){
    c->lives--;
    if(c->lives <= 0 && c->inGame){
        playersInGame--;
        c->inGame=false;
        if(playersInGame==1){
            endGame();
        }
    }
    else{
        c->isJumping=false;
        c->damage=1;
        c->xvel=0;
        c->yvel=0;
        c->xvelmov=0;
        c->spin=0;
        c->spinvel=0;
        c->spinvelmov=0;
        c->xcenter=0;
        c->ycenter=0;
        if(c->hasGun){
            unEquipGun(c);
        }
    }
    /*
        int playerNumber;
    GRRLIB_texImg *playerIMG;
    int xpos;
    int ypos;
    int xcenter;
    int ycenter;
    int jump;
    bool isJumping;
    int timeInAir;
    u16 down;
    u16 held;
    bool gcController;
    int controllerNumber;
    float damage;
    float xvel;
    float yvel;
    float xvelmov;
    float spin;
    float spinvel;
    float spinvelmov;
    bool hasGun;
    struct gun playerGun;
    float cStickDegree;
    bool inGame;
    int lives;
    */
}

void playerControl(struct character *c){
    if(c->gcController){
        c->down=PAD_ButtonsDown(c->controllerNumber);
        c->held=PAD_ButtonsHeld(c->controllerNumber);

        if(c->down & PAD_BUTTON_A){
            if(!c->hasGun){
                pickupGun(c);
            }
        }

        if(c->down & PAD_TRIGGER_R){
            if(c->hasGun){
                shoot(*c);
                unEquipGun(c);
            }
        }
    }
    else{
        c->down=WPAD_ButtonsDown(c->controllerNumber);
        c->held=WPAD_ButtonsHeld(c->controllerNumber);
    }

    bool isTouchingBox=false;
    for(int i=0;i<boxesSize;i++){
        if(touchingBox(boxes[i], *c)){
            isTouchingBox=true;
            //c->ypos=boxes[i].y-52;//cos(degToRad(c->spin+315))*73.53910524;
        }
    }


    if(c->gcController){
        if(PAD_StickY(c->controllerNumber)>=36) {
            if(c->jump<2 && !c->isJumping){
                c->jump++;
                c->ycenter-=20;
                c->isJumping=true;
                c->timeInAir=0;
            }
        }
        if(c->isJumping && c->timeInAir<=15){
            if(PAD_StickY(c->controllerNumber)>=36) {
                c->timeInAir++;
                c->ycenter-=10;
            }
            else{
                c->isJumping=false;
            }
        }
        if(abs(PAD_StickX(c->controllerNumber))>=2) {
            c->xvelmov+=PAD_StickX(c->controllerNumber)/18;
        }
    }
    else{
        if(c->down & WPAD_BUTTON_UP) {
            if(c->jump<2){
                c->jump++;
                c->ycenter-=20;
                c->isJumping=true;
                c->timeInAir=0;
            }
        }
        if(c->isJumping && c->timeInAir<=15){
            if(c->held & WPAD_BUTTON_UP) {
                c->timeInAir++;
                c->ycenter-=10;
            }
            else{
                c->isJumping=false;
            }
        }
        if(c->held & WPAD_BUTTON_LEFT) {
            c->xvelmov-=1;
        }
        if(c->held & WPAD_BUTTON_RIGHT) {
            c->xvelmov+=1;
        }
    }
    if(!isTouchingBox){
        c->yvel+=0.25;
    }
    else{
        c->jump=0;
        c->isJumping=false;
        if(c->yvel>0){
            c->yvel=0;
        }
    }
    c->spin+=c->spinvelmov;
    c->spin+=c->spinvel;

    if(c->xvelmov>5){
        c->xvelmov=5;
    }
    if(c->xvelmov<-5){
        c->xvelmov=-5;
    }

    if(isTouchingBox){
        c->spinvelmov+=c->xvelmov;
        c->spinvelmov+=c->xvel;
    }
    if(c->spinvelmov<-45){
        c->spinvelmov=-45;
    }
    else if(c->spinvelmov>45){
        c->spinvelmov=45;
    }

    c->xcenter+=c->xvelmov;
    if(isTouchingBox){
        c->xvel-=c->xvel*.25;
        c->xvelmov-=c->xvelmov*.25;
        c->spinvel-=c->spinvel*.1;
        c->spinvelmov-=c->spinvelmov*.1;
    }
    else{
        c->xvel-=c->xvel*.1;
        c->xvelmov-=c->xvelmov*.1;
        c->spinvel-=c->spinvel*.05;
        c->spinvelmov-=c->spinvelmov*.05;
    }

    if(c->ycenter>yend+4*playerSize || c->xcenter<0-4*playerSize || c->xcenter>xend+4*playerSize){
        killPlayer(c);
    }

    c->xcenter+=c->xvel;
    c->ycenter+=c->yvel;
    /*c->xcenter=c->xpos-sin(degToRad(c->spin+315))*36.76955262;
    c->ycenter=c->ypos+cos(degToRad(c->spin+315))*36.76955262;*/
    c->ypos=c->ycenter-cos(degToRad(c->spin+315))*sqrt(playerSize*playerSize*2);
    c->xpos=c->xcenter+sin(degToRad(c->spin+315))*sqrt(playerSize*playerSize*2);
    GRRLIB_DrawImg(c->xpos, c->ypos, c->playerIMG, c->spin, 1, 1, playerColors[c->controllerNumber]);//GRRLIB_WHITE);
    
    if(c->hasGun){
        float cstickx=PAD_SubStickX(c->controllerNumber);
        float csticky=PAD_SubStickY(c->controllerNumber);
        if(abs(csticky)>=62 || abs(cstickx)>=62){
            c->cStickDegree=atan2(csticky,cstickx);
        }
        int tempx=c->xcenter+cos(c->cStickDegree)*playerSize-sin(c->cStickDegree)*gunSize;
        int tempy=c->ycenter-sin(c->cStickDegree)*playerSize-cos(c->cStickDegree)*gunSize;
        GRRLIB_DrawImg(tempx, tempy, (c->playerGun).gunIMG, -1*radToDeg(c->cStickDegree), 1, 1, GRRLIB_WHITE);
    }
}

void spawnGun(){
    struct gun basicBlackGun;
    gun_INIT(&basicBlackGun, BasicBlackGun, 0.2f, 10, 0, 45, 150);
    basicBlackGun.index=amountOfGunsInGame;
    basicBlackGun.ycenter=rand()%150+150;
    basicBlackGun.xcenter=rand()%440+100;
    gunsInGame[amountOfGunsInGame]=basicBlackGun;
    amountOfGunsInGame++;
}

void printGuns(){
    for(int i=0;i<amountOfGunsInGame;i++){
        int ypos=gunsInGame[i].ycenter-cos(sqrt(gunSize*gunSize*2));
        int xpos=gunsInGame[i].xcenter+sin(sqrt(gunSize*gunSize*2));
        GRRLIB_DrawImg(xpos, ypos, gunsInGame[i].gunIMG, 0, 1, 1, GRRLIB_WHITE);
    }
}

const int waitTime=1;
double timeCounter=waitTime;
void gunSpawnTimer(){
    if(playersInGame+1>amountOfGunsInGame){
        timeCounter-=deltaTime;
        if(timeCounter<=0){
            spawnGun();
            timeCounter=rand()%waitTime+waitTime;
        }
    }
}

int main() {
    srand(gettime());
    //settime((uint64_t)0); //So we don't have to start with a huge number.

    WPAD_Init();
    PAD_Init();
    //WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    GRRLIB_Init();

    ASND_Init();
	MP3Player_Init();
    //MP3Player_Volume(255);
    ASND_Pause(0);
    MP3Player_Volume(255);
    //ASND_Pause(0);

    boxInit(&boxes[0],100,350,440,5);

    boxInit(&boxes[1],150,250,100,5);

    boxInit(&boxes[2],390,250,100,5);

    boxInit(&boxes[3],270,150,100,5);

    GRRLIB_texImg *AButton=GRRLIB_LoadTexture(PressA);
    GRRLIB_texImg *BButton=GRRLIB_LoadTexture(PressB);

    GRRLIB_texImg *winnerImg[4]={GRRLIB_LoadTexture(Player1Wins),GRRLIB_LoadTexture(Player2Wins),GRRLIB_LoadTexture(Player3Wins),GRRLIB_LoadTexture(Player4Wins)};

    for(int i=0;i<4;i++){
        player[i].inGame=false;
    }

    MP3Player_PlayBuffer(OpeningSong_mp3, OpeningSong_mp3_size, NULL);
    GRRLIB_SetBackgroundColour(0x00, 0xFF, 0xFF, 0xFF);
    while(1) {
        deltaTime = (double)(gettime() - deltaTimeStart) / (double)(TB_TIMER_CLOCK * 1000); // division is to convert from ticks to seconds
		deltaTimeStart = gettime();

        WPAD_ScanPads();
        PAD_ScanPads();

        if(scene==1){
            u16 playerDown[4];

            if(!MP3Player_IsPlaying()){
                MP3Player_PlayBuffer(OpeningSong_mp3, OpeningSong_mp3_size, NULL);
            }

            for(int i=0;i<4;i++){
                playerDown[i]=PAD_ButtonsDown(i);

                if(player[i].inGame){
                    GRRLIB_DrawImg(128-52+128*i, 240-104, BButton, 0, 1, 1, GRRLIB_WHITE);
                    if(playerDown[i] & PAD_BUTTON_B){
                        player[i].inGame=false;
                        playersInGame--;
                    }
                }
                else{
                    GRRLIB_DrawImg(128-52+128*i, 240-104, AButton, 0, 1, 1, GRRLIB_WHITE);
                    if(playerDown[i] & PAD_BUTTON_A){
                        player_INIT(&(player[i]), PlayerFace, true, i, i+1, 1);
                    }
                }

                if(playerDown[i] & PAD_BUTTON_START){
                    if(playersInGame>=2){
                        MP3Player_Stop();
                        MP3Player_PlayBuffer(InGameSong_mp3, InGameSong_mp3_size, NULL);
                        scene=2;
                    }
                }
            }

            
        }
        
        if(scene==2){
            if(!MP3Player_IsPlaying()){
                MP3Player_PlayBuffer(InGameSong_mp3, InGameSong_mp3_size, NULL);
            }

            for(int i=0;i<boxesSize;i++){
                GRRLIB_Rectangle(boxes[i].x, boxes[i].y, boxes[i].width, boxes[i].height, GRRLIB_RED, true);
            }

            for(int i=0;i<4;i++){
                if(player[i].inGame){
                    playerControl(&(player[i]));
                }
            }
            printGuns();
            gunSpawnTimer();
        }

        if(scene==3){
            u16 playerDown[4];

            if(!MP3Player_IsPlaying()){
                MP3Player_PlayBuffer(WinSong_mp3, WinSong_mp3_size, NULL);
            }

            GRRLIB_DrawImg(320-250, 240-72, winnerImg[winner], 0, 1, 1, GRRLIB_WHITE);
            for(int i=0;i<4;i++){
                playerDown[i]=PAD_ButtonsDown(i);
                if(playerDown[i] & PAD_BUTTON_START){
                    GRRLIB_SetBackgroundColour(0x00, 0xFF, 0xFF, 0xFF);
                    MP3Player_Stop();
                    MP3Player_PlayBuffer(OpeningSong_mp3, OpeningSong_mp3_size, NULL);
                    scene=1;
                }
            }
        }
        GRRLIB_Render();
    }
    GRRLIB_Exit();
    exit(0);
}