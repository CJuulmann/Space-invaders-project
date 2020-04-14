
/************************************************************************/
/*	Hardware/Sofwtare programming course project @ DTU					*/
/*	Author: Christina Juulmann											*/
/* 																		*/		
/*	Last modified: 23th of January 2019									*/
/************************************************************************/

#ifndef VIDEO_DEMO_H_
#define VIDEO_DEMO_H_

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "xil_types.h"

/* ------------------------------------------------------------ */
/*					Miscellaneous Declarations					*/
/* ------------------------------------------------------------ */

#define SCREEN_STATE_1 1
#define SCREEN_STATE_2 2
#define SCREEN_STATE_3 3

#define DEMO_MAX_FRAME (640*480*3)
#define DEMO_STRIDE (640 * 3)
#define STRIDE (480 * 3)

#define TRUE 1
#define ALIEN_BLT_N 10



/*
 * Configure the Video capture driver to start streaming on signal
 * detection
 */
#define DEMO_START_ON_DET 1

/* ------------------------------------------------------------ */
/*					Structure Declarations						*/
/* ------------------------------------------------------------ */

typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
	double health;
} Hero;


typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
	u8 fired;
	u8 impact; //0 = still moving, 1 = impact enemy, 2 = impact boundary
} Bullet;

typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
	u8 alive;
	Bullet eBullets[ALIEN_BLT_N];
	u8 impact;
} Enemy;

typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
} HeroHealth;

typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
} GameOver;

typedef struct {
	u32 ytop;
	u32 ybtm;
	u32 xLeft;
	u32 xRight;
} StartGame;

enum movedir {
    ydown = 1,
	xleft = 2,
    xright = 3,
} dir;

enum last{
	left = 1,
	right = 2,
} lastEdge;

enum gameState{
	startgame = 1,
	ingame = 2,
	gameover = 3,
} gameState;


/* ------------------------------------------------------------ */
/*					Procedure Declarations						*/
/* ------------------------------------------------------------ */

void DemoInitialize();
void DemoRun();
void DemoPrintMenu();
void DemoChangeRes();
void DemoCRMenu();
void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride);
void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int screen_state);
void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride);
void DemoISR(void *callBackRef, void *pVideo);

// set game object coordinates
void DefineAlien(u32 width, u32 height, u8 step);
void DefineHero(u32 width, u32 height, s32 skew);
void DefineHeroHealth();
void FireBullet();
void UpdateBullet();
void FireEBullet(u8 alienidx);
void UpdateEBullet();

// draw game objects to screen
void DrawHero(u8 *frame);
void DrawHeroHealth(u8 *frame);
void DrawAlien(u8 *frame);
void DrawBullet(u8 *frame);
void DrawEBullet(u8 *frame);

// screen/buffers
void SwapBuf();
void clearScreen();
void DrawGameOver(u8 *frame);
void DrawStartGame(u8 *frame);


/* ------------------------------------------------------------ */

/************************************************************************/

#endif /* VIDEO_DEMO_H_ */
