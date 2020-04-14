/************************************************************************/
/*	Hardware/Sofwtare programming course project @ DTU					*/
/*	Author: Christina Juulmann											*/
/* 																		*/		
/*	Last modified: 23th of January 2019									*/
/************************************************************************/


/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "video_demo.h"		// structure and prodecure definitions
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "timer_ps/timer_ps.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "interrupts.h"		//interupts lib
#include "xtime_l.h"
#include "sprites.h"



/* ------------------------------------------------------------ */
/*				Defines	macro definitions						*/
/* ------------------------------------------------------------ */

/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_IRPT_ID XPS_FPGA3_INT_ID
#define VID_GPIO_IRPT_ID XPS_FPGA4_INT_ID
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR

/*
 * Timer definitions
 */
#define TMR_LOAD				833333 //counts per millisecond
#define INTC_TMR_INTERRUPT_ID 	XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR
#define TMR_DEVICE				XPAR_TMRCTR_0_DEVICE_ID

/*
 * Array sizes
 */
#define BULLET_N 100
#define ALIENS_N 30
#define ALIENS_STRIDE 10
#define HERO_N 1
#define LVL_2

/*
 * Game modes defines
 */

#ifdef LVL_0
	#define HERO_HEALTH 10
	#define DMG 0.1
	#define PROB 0.02
	#define EVENTCTRL 32
#endif
#ifdef LVL_1
	#define HERO_HEALTH 5
	#define DMG 0.2
	#define PROB 0.04
	#define EVENTCTRL 16
#endif
#ifdef LVL_2
	#define HERO_HEALTH 2
	#define DMG 0.5
	#define PROB 0.06
	#define EVENTCTRL 12
#endif

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */


/*
 * Framebuffers for video/vga data out
 */
u8 frameBufA[DEMO_MAX_FRAME];	//front-buffer (read from)
u8 frameBufB[DEMO_MAX_FRAME];	//back-buffer (write to)
u8 *ptr_frameBufA;
u8 *ptr_frameBufB;

Enemy alienArr[ALIENS_N];
Hero heroArr[HERO_N];
Bullet bulletArr[BULLET_N];
HeroHealth heroHealth[HERO_HEALTH];

s32 skew;		//player skew
u8 trigger;		//shot flag

u32 nestLeft, nestRight, nestTop, nestBtm;	//alien block control param
u32 rightEdge, leftEdge, btmEdge;
u16 i, j;
u8 cnt;
u32 bcnt;
double r;

/*
 * Display and Video Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect

/*
 * Levels
 */
u8 hero_health;
s8 dmg;

int main(void)
{
	/* Interrupt initialization */
	int status;
	// Initialize Push Buttons
	status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Set all buttons direction to inputs
	XGpio_SetDataDirection(&BTNInst, 1, 0xFF);

	status = IntcInitFunction(INTC_DEVICE_ID, &BTNInst);
	if(status != XST_SUCCESS) return XST_FAILURE;


	DemoInitialize();					//init device drivers for screen
	ptr_frameBufB = frameBufB;			//set pointer to back-buffer

	//init test gamestate
	gameState = startgame;

	//Infinite loop: switches between game states
	while(TRUE){
		//========================================================
		// STATE: START-GAME
		//========================================================
		clearScreen();
		DrawStartGame(frameBufB);
		SwapBuf();

		//initial lvl chosen to easy
		//hero_health = 10;
		//dmg = 0.1;

		while(gameState == startgame){
			if(btns == button4){
				gameState = ingame;
				break;						//break startgame state
			}
			/*if(btns == button3){
				//choose LVL_0
				hero_health = 10;
				dmg = 0.1;
			}
			if(btns == button2){
				//choose LVL_1
				hero_health = 5;
				dmg = 0.2;
			}
			if(btns == button1){
				//choose LVL_2
				hero_health = 2;
				dmg = 0.5;
			}*/
		}

		//========================================================
		// STATE: IN-GAME
		//========================================================
		while(gameState == ingame){
			/*
			 * Initialize game objects
			 */
			clearScreen();					//clear screen

			srand(10);						//make seed for random num gen
			skew = 0;						//init incrementing value for alien movement
			trigger = 0;
			cnt = 0;
			bcnt = 32;

			for(i=0; i<BULLET_N; i++){
				bulletArr[i].fired = 0;
				bulletArr[i].impact = 0;
			}
			for(i=0; i<ALIENS_N; i++){
				alienArr[i].alive = 1;
				for(j=0; j<ALIEN_BLT_N; j++){
					alienArr[i].eBullets[j].fired = 0;
				}
			}
			for(i=0;i<HERO_N;i++){
				heroArr[i].health = 1;
			}

			lastEdge = left;
			leftEdge = 96;
			rightEdge = 1920-96;

			DefineAlien(dispCtrl.vMode.width, dispCtrl.vMode.height, 0);
			DefineHeroHealth();

			/*
			 * Run game (until break happens in GAMEOVER state)
			 */
			while(TRUE){
				cnt++;			//event control for alien movement

				DefineHero(dispCtrl.vMode.width, dispCtrl.vMode.height, skew);
				DrawHeroHealth(ptr_frameBufB);

				if(trigger == 1){
					trigger = 0; 			//reset trigger
					if(bcnt >31){
						FireBullet();
						bcnt = 0;
					}
				}
				bcnt++;						//event control for bullet fire

				UpdateBullet();

				nestLeft = alienArr[0].xLeft;
				nestRight = alienArr[ALIENS_N-1].xRight;
				nestTop = alienArr[0].ytop;
				nestBtm = alienArr[ALIENS_N-1].ybtm;

				if(nestRight == rightEdge){
					dir = ydown;
				} else if(nestLeft == leftEdge){
					dir = ydown;
				}else if(nestRight < rightEdge){
					if(lastEdge==left)
						dir = xright;
					else
						dir = xleft;
				}

				if(cnt==EVENTCTRL){	//event interval for alien control (move and fire)
					for(i=0; i<ALIENS_STRIDE; i++){
						if(alienArr[i+20].alive == 1){
							r = (double)rand()/RAND_MAX;						//random num and scale down to num between 0 - 1
							if(r < PROB){										//5% probability that an alien will shoot
								FireEBullet(i+20);
							}
						}else if(alienArr[i+10].alive == 1){
							r = (double)rand()/RAND_MAX;						//random num and scale down to num between 0 - 1
							if(r < PROB){										//5% probability that an alien will shoot
								FireEBullet(i+10);
							}
						}else if(alienArr[i].alive == 1){
							r = (double)rand()/RAND_MAX;						//random num and scale down to num between 0 - 1
							if(r < PROB){										//5% probability that an alien will shoot
								FireEBullet(i);
							}

						}
					}
					switch(dir){
						case ydown:
								for(i=0; i<ALIENS_N; i++){
									alienArr[i].ytop+=16;
									alienArr[i].ybtm+=16;
									if (lastEdge == left){
										alienArr[i].xLeft-=3;
										alienArr[i].xRight-=3;
									}else{
										alienArr[i].xLeft+=3;
										alienArr[i].xRight+=3;
									}
								}
								if (lastEdge == left){
									lastEdge = right;
								}else{
									lastEdge = left;
								}
							break;
						case xright:
								for(i=0; i<ALIENS_N; i++){
									alienArr[i].xLeft+=3;
									alienArr[i].xRight+=3;
								}
								lastEdge = left;
							break;
						case xleft:
								for(i=0; i<ALIENS_N; i++){
									alienArr[i].xLeft-=3;
									alienArr[i].xRight-=3;
								}
								lastEdge = right;
							break;
						default:
							break;
					}
					cnt = 0;	//delay/event functionality for alien control reset
				}
				UpdateEBullet();

				DrawHero(ptr_frameBufB);
				DrawBullet(ptr_frameBufB);
				DrawAlien(ptr_frameBufB);
				DrawEBullet(ptr_frameBufB);

				SwapBuf();					//copy back-buffer into front-buffer

				if(btns == button1){
					skew += 1;				//move right
				}
				if(btns == button2){
				}
				if(btns == button3){
					trigger = 1;			//shot fired
				}
				if(btns == button4){
					skew -= 1;				//move left
				}

				//check for game over
				if(nestBtm >= heroArr[0].ytop || heroArr[0].health <= 0.005){  //should be <= 0 but due to rounding error, set it slightly above 0
					DrawGameOver(ptr_frameBufB);
					SwapBuf();
					gameState = gameover;
					//========================================================
					// STATE: GAMEOVER - replay?
					//========================================================
					while(gameState == gameover){
						if(btns == button1){
							gameState = startgame;
							break;	//break out of gameover
						}
					}
					break; // break out of while true and go to while in-game (only if game over)
				}//end if

			}//while true
			break;	// break out of while ingame
		} //while ingame

	}// outer while true (never breaks)
	return 0;
}
void BTN_Intr_Handler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTNInst, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) !=
			BTN_INT) {
			return;
		}

	btns = XGpio_DiscreteRead(&BTNInst, 1);

    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}
void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;

	//initialize pointer to front-buffer (frameBufA)
	ptr_frameBufA = frameBufA;

	/*
	 * Initialize a timer used for a simple delay
	 */
	TimerInitialize(SCU_TIMER_ID);


	// Initialize VDMA driver
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, &ptr_frameBufA, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	return;
}
void SwapBuf(){

	int frameBufLen;
	frameBufLen = sizeof(frameBufB);

	memcpy(frameBufA, frameBufB, frameBufLen);							//copy frame from back-buffer to front-buffer
	memset(frameBufB, 0, frameBufLen);									//clear back-buffer
	Xil_DCacheFlushRange((unsigned int) frameBufA, DEMO_MAX_FRAME);		//print from  front-buffer
}
void clearScreen(){
	int frameBufLen;
	frameBufLen = sizeof(frameBufB);

	memset(frameBufB, 0, frameBufLen);									//clear back-buffer
	memcpy(frameBufA, frameBufB, frameBufLen);							//copy frame from back-buffer to front-buffer
	Xil_DCacheFlushRange((unsigned int) frameBufA, DEMO_MAX_FRAME);		//print from  front-buffer
}

/*
 * Define coordinates of game objects
 */
void DefineAlien(u32 width, u32 height, u8 step){

	u8 i;

	u32 wh;						//width/height of enemy (is quadratic)
	u32 offset;					//offset of enemy relative to the enemy on the left hand side
	u32 margin;					//margin for aliens from screen edges

	//init param
	wh = 32;
	margin = 2*wh;
	offset = (width-margin-(ALIENS_N/3)*wh)/((ALIENS_N/3)+1);


			//initialize alien positions in top of screen (3 rows with 10 aliens in each row)
			for(i=0; i<ALIENS_N; i++){
				if(i < 10){
					alienArr[i].xLeft = ((i+1)*3*offset+(3*margin/2)+i*3*wh);				//first row
					alienArr[i].xRight = alienArr[i].xLeft + 3*wh;
					alienArr[i].ytop = 0.125*height-0.5*wh+step;
					alienArr[i].ybtm = alienArr[i].ytop+wh;
				}else if(i < 20){
					alienArr[i].xLeft = ((i+1-10)*3*offset+(3*margin/2)+(i-10)*3*wh);		//second row
					alienArr[i].xRight = alienArr[i].xLeft + 3*wh;
					alienArr[i].ytop = 0.125*height-0.5*wh+2*wh+step;
					alienArr[i].ybtm = alienArr[i].ytop+wh;
				}else {
					alienArr[i].xLeft = ((i+1-20)*3*offset+(3*margin/2)+(i-20)*3*wh);		//third row
					alienArr[i].xRight = alienArr[i].xLeft + 3*wh;
					alienArr[i].ytop = 0.125*height-0.5*wh+4*wh+step;
					alienArr[i].ybtm = alienArr[i].ytop+wh;
				}
			}
}
void DefineHero(u32 width, u32 height, s32 skew){

	u8 i;
	u32 wh;
	u8 margin;
	u32 xMin;
	u32 xMax;

	//init param
	wh = 32;		//wh=width/height of hero (quadratic)
	margin = 2*wh;
	xMin = 192;//margin;
	xMax = 1728;//width-margin-wh;

	for(i=0; i<HERO_N; i++){

		//control if player moves outside of screen
		heroArr[i].xLeft = (0.5*width-0.5*wh)*3;			//xLeft is controlling param and initially set to middle of screen

		if(heroArr[i].xLeft+skew*3 < xMin){					//if skew+init position < xMin then xLeft = xMin
			heroArr[i].xLeft = xMin;
		}
		else if(heroArr[i].xLeft+skew*3 > xMax){			//if skew+init position > xMax then xLeft = xMax
			heroArr[i].xLeft = xMax;

		} else {
			heroArr[i].xLeft += 3*skew;
		}
		heroArr[i].xRight = heroArr[i].xLeft + wh*3;
		heroArr[i].ytop = height-0.75*margin;
		heroArr[i].ybtm = heroArr[i].ytop+wh;
	}
}
void FireEBullet(u8 alienidx){
	u8 i;
	for(i=0; i<	ALIEN_BLT_N; i++){
		if(alienArr[alienidx].eBullets[i].fired == 0)	{	//if free bullet and alien is alive and is in the front line
			alienArr[alienidx].eBullets[i].fired = 1;
			alienArr[alienidx].eBullets[i].xLeft = 0.5*(alienArr[alienidx].xLeft+alienArr[alienidx].xRight)-3;
			alienArr[alienidx].eBullets[i].xRight = alienArr[alienidx].eBullets[i].xLeft+2;
			alienArr[alienidx].eBullets[i].ybtm = alienArr[alienidx].ybtm+4;
			alienArr[alienidx].eBullets[i].ytop = alienArr[alienidx].ybtm;
			alienArr[alienidx].impact = 0;
			break;
		}
	}
}
void FireBullet(){
	u8 i;
	for(i=0; i<BULLET_N; i++){
		if(bulletArr[i].fired == 0){
			bulletArr[i].fired = 1;
			bulletArr[i].xLeft = 0.5*(heroArr[0].xLeft+heroArr[0].xRight)-3;
			bulletArr[i].xRight = bulletArr[i].xLeft+2;
			bulletArr[i].ytop = heroArr[0].ytop-4;
			bulletArr[i].ybtm = heroArr[0].ytop;
			bulletArr[i].impact = 0;
			//bulletArr[i].killidx = -1;
			break;
		}
	}
}
void UpdateEBullet(){
	u8 i, j;

	for(i=0; i<ALIENS_N; i++){
		for(j=0; j<ALIEN_BLT_N; j++){
			if(alienArr[i].eBullets[j].fired == 1){		//if bullet fired check for collision
				alienArr[i].eBullets[j].ybtm++;
				alienArr[i].eBullets[j].ytop++;

				//collision detect
				if(alienArr[i].eBullets[j].ybtm == heroArr[0].ytop &&
				   alienArr[i].eBullets[j].xLeft >= heroArr[0].xLeft &&
				   alienArr[i].eBullets[j].xRight <= heroArr[0].xRight){		//check for hero impact
						alienArr[i].eBullets[j].impact = 1;						//hero impact
						alienArr[i].eBullets[j].fired = 0;						//reset bullet
						heroArr[0].health -= DMG;								//damage percentage
						break;
				} else if(alienArr[i].eBullets[j].ybtm == 480){
						alienArr[i].eBullets[j].impact = 2;						//edge impact
						alienArr[i].eBullets[j].fired = 0;

				} else{															//no impact (still flying)
					alienArr[i].eBullets[j].impact = 0;
				}
			}
		}
	}
}
void UpdateBullet(){
	u8 i,j;

	for(i=0; i<BULLET_N; i++){
		if(bulletArr[i].fired == 1){								//if bullet is fired we check for collision
			bulletArr[i].ytop--;									//update ypos
			bulletArr[i].ybtm--;

			//collision detection
			if(bulletArr[i].ytop <= alienArr[ALIENS_N-1].ybtm ||
			   bulletArr[i].ytop <= alienArr[ALIENS_N-11].ybtm ||
			   bulletArr[i].ytop <= alienArr[ALIENS_N-21].ybtm){	//bullet ypos has reached alien lines (potential impact)

				for(j=0; j<ALIENS_N; j++){
					if(alienArr[j].alive == 1){						//collision detect only on alive aliens

						if(bulletArr[i].xLeft >= alienArr[j].xLeft &&		//check for alien impact
						   bulletArr[i].xRight <= alienArr[j].xRight &&
						   bulletArr[i].ytop == alienArr[j].ybtm){

								bulletArr[i].impact = 1;				//got alien impact
								bulletArr[i].fired = 0;
								//bulletArr[i].ytop = 500; 				//reset bullet position
								//bulletArr[i].ybtm = 456;
								alienArr[j].alive = 0;
								break;

						} else if(bulletArr[i].ytop == 0) {				//check for boundary impact

								bulletArr[i].impact = 2;				//got boundary impact
								bulletArr[i].fired = 0;
								//bulletArr[i].ytop = 500; 	//reset bullet position
								//bulletArr[i].ybtm = 456;
						} else {							//missed aliens and still flying (not reached edges yet)

								bulletArr[i].impact = 0;
						}
					}
				}
			}
		}

	}
}
void DefineHeroHealth(){
	u8 i;
	u32 wh, offset, height;						//width/height of health icons, offset of health icons, screen width/height

	wh = 15;
	height = dispCtrl.vMode.height;
	offset = 0.5*wh;

	for(i=0; i<HERO_HEALTH; i++){
		heroHealth[i].xLeft = (i*wh+(i+1)*offset)*3+offset*3;//width-(i+1)*3*offset+i*3*wh;
		heroHealth[i].xRight = heroHealth[i].xLeft+3*wh;
		heroHealth[i].ytop = height-wh;
		heroHealth[i].ybtm = heroHealth[i].ytop+wh;
	}
}

/*
 * Drawing to screen
 */
void DrawStartGame(u8 *frame){
	//screen
	u32 xcoi, ycoi, wh;				//x,y coordinates of screen
	u32 iPixelAddr;					//running index of pixel addresses

	u32 iSprite, width, height;
	StartGame startGame;

	iSprite = 0;
	width = dispCtrl.vMode.width;
	height = dispCtrl.vMode.height;
	wh = 200;

	startGame.xLeft = 0.5*width*3-0.5*wh*3;
	startGame.ytop = 0.5*height-0.5*wh;


	for(xcoi=0; xcoi<(wh*3); xcoi+=3){

		iPixelAddr = startGame.ytop*DEMO_STRIDE+startGame.xLeft+xcoi;

		for(ycoi=0; ycoi<wh;ycoi++){
			frame[iPixelAddr] = start_game[iSprite];
			frame[iPixelAddr+1] = start_game[iSprite+1];
			frame[iPixelAddr+2] = start_game[iSprite+2];

			iPixelAddr += DEMO_STRIDE;
			iSprite += 3;
		}
	}
}
void DrawGameOver(u8 *frame){

	//screen
	u32 xcoi, ycoi, wh;				//x,y coordinates of screen
	u32 iPixelAddr;					//running index of pixel addresses

	u32 iSprite, width, height;
	GameOver gameOver;

	iSprite = 0;
	width = dispCtrl.vMode.width;
	height = dispCtrl.vMode.height;
	wh = 200;

	gameOver.xLeft = 0.5*width*3-0.5*wh*3;
	gameOver.ytop = 0.5*height-0.5*wh;


	for(xcoi=0; xcoi<(wh*3); xcoi+=3){

		iPixelAddr = gameOver.ytop*DEMO_STRIDE+gameOver.xLeft+xcoi;

		for(ycoi=0; ycoi<wh;ycoi++){
			frame[iPixelAddr] = game_over[iSprite];
			frame[iPixelAddr+1] = game_over[iSprite+1];
			frame[iPixelAddr+2] = game_over[iSprite+2];

			iPixelAddr += DEMO_STRIDE;
			iSprite += 3;
		}
	}
}
void DrawHero(u8 *frame){

	u8 i;
	u32 wh;

	//screen
	u32 xcoi, ycoi;				//x,y coordinates of screen
	u32 iPixelAddr;				//running index of pixel addresses

	u32 iSprite;

	for(i=0; i<HERO_N; i++){
		wh = heroArr[i].ybtm-heroArr[i].ytop;
		iSprite = 0;

			for(xcoi=0; xcoi<wh*3; xcoi+=3){

				iPixelAddr = (heroArr[i].ytop-1)*DEMO_STRIDE+heroArr[i].xLeft + xcoi;

					for(ycoi=0; ycoi<wh;ycoi++){
							frame[iPixelAddr] = laser_cannon[iSprite];
							frame[iPixelAddr+1] = laser_cannon[iSprite+1];
							frame[iPixelAddr+2] = laser_cannon[iSprite+2];

						iPixelAddr += DEMO_STRIDE;
						iSprite += 3;
					}
			}
	}
}
void DrawAlien(u8 *frame){
	u8 i;
	u32 wh;
	u32 xcoi, ycoi, iPixelAddr;
	u32 iSprite;

	wh = 32;	// wh = alienArr[i].ybtm-alienArr[i].ytop; => always 32, save calculation and set constant value instead

		//checking for all aliens coordinates (in oppose for all pixels on screen)
		for(i=0;i<ALIENS_N; i++){
			if(alienArr[i].alive == 1){
				iSprite = 0;

				for(xcoi=0;xcoi<wh*3;xcoi+=3){
					iPixelAddr = (alienArr[i].ytop-1)*DEMO_STRIDE+alienArr[i].xLeft + xcoi;

					for(ycoi=0;ycoi<wh;ycoi++){
						if(i < 10){
								frame[iPixelAddr] = octopus_invader[iSprite];
								frame[iPixelAddr+1] = octopus_invader[iSprite+1];
								frame[iPixelAddr+2] = octopus_invader[iSprite+2];
						}else if(i<20){
								frame[iPixelAddr] = cuttlefish_invader[iSprite];
								frame[iPixelAddr+1] = cuttlefish_invader[iSprite+1];
								frame[iPixelAddr+2] = cuttlefish_invader[iSprite+2];
						} else{
								frame[iPixelAddr] = crab_invader[iSprite];
								frame[iPixelAddr+1] = crab_invader[iSprite+1];
								frame[iPixelAddr+2] = crab_invader[iSprite+2];
						}
						iPixelAddr += DEMO_STRIDE;
						iSprite += 3;
					}
				}
			}
		}
}
void DrawEBullet(u8 *frame){
	u8 w,h, i, j;
	u32 xcoi, ycoi, iPixelAddr;

	//init param
	w = 2;
	h = 4;
	for(i=0; i<ALIENS_N; i++){
		for(j=0; j<ALIEN_BLT_N; j++){
			if(alienArr[i].eBullets[j].impact == 0){

				for(xcoi=0; xcoi<w*3; xcoi+=3){
					iPixelAddr = (alienArr[i].eBullets[j].ytop-1)*DEMO_STRIDE+alienArr[i].eBullets[j].xLeft + xcoi;

						for(ycoi=0; ycoi<h;ycoi++){
							frame[iPixelAddr] = 255; 	//G
							frame[iPixelAddr+1] = 255; 	//B
							frame[iPixelAddr+2] = 255; 	//R

							iPixelAddr += DEMO_STRIDE;
						}
				}
			}
		}
	}
}
void DrawBullet(u8 *frame){
	u8 w, h, i;
	u32 xcoi, ycoi, iPixelAddr;

	//init param
	w = 2;
	h = 4;

	for(i=0; i<BULLET_N; i++){
		if(bulletArr[i].impact == 0){
			for(xcoi=0; xcoi<w*3; xcoi+=3){
				iPixelAddr = (bulletArr[i].ytop-1)*DEMO_STRIDE+bulletArr[i].xLeft + xcoi;

					for(ycoi=0; ycoi<h;ycoi++){
						frame[iPixelAddr] = 255; 	//G
						frame[iPixelAddr+1] = 0; 	//B
						frame[iPixelAddr+2] = 0; 	//R

						iPixelAddr += DEMO_STRIDE;
					}
			}
		}
	}
}
void DrawHeroHealth(u8 *frame){
	u8 i;
	u32 wh;
	double healthsize;

	//screen
	u32 xcoi, ycoi;				//x,y coordinates of screen
	u32 iPixelAddr;				//running index of pixel addresses

	u32 iSprite;
	healthsize = (double) 1/HERO_HEALTH;
	for(i=0; i<HERO_HEALTH; i++){
		wh = heroHealth[i].ybtm-heroHealth[i].ytop;
		iSprite = 0;

		if(heroArr[0].health >= (i+1)*healthsize){

			for(xcoi=0; xcoi<wh*3; xcoi+=3){
				iPixelAddr = (heroHealth[i].ytop-1)*DEMO_STRIDE+heroHealth[i].xLeft + xcoi;

				for(ycoi=0; ycoi<wh;ycoi++){
					frame[iPixelAddr] = 255;
					frame[iPixelAddr+1] = 0;
					frame[iPixelAddr+2] = 0;

					iPixelAddr += DEMO_STRIDE;
					iSprite += 3;
				}
			}
		}else{
			break;
		}
	}
}
