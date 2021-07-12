#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdio.h>

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define BUFLEN 512
#define NPACK 10
#define PORT 9930
//#define SRV_IP "192.168.1.115"
char SRV_IP[16];

#define SERVER 0x01
#define CLIENT 0x02



#include "TicTacToeMsgs.h"
#include "TicTacToe.h"

PSP_MODULE_INFO("Net Tic", 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);
#define printf pspDebugScreenPrintf
/*************************************************************/
void diep(char *s)
{
  perror(s);
  exit(1);
}

/*************************************************************/
int BuildIt(char _a, char _b)
{
  int y = 0;
  y = (_b<<8) | (_a & 0x00ff);
//  printf("Decoded y: %d\n", y);
  return y;
}

/*************************************************************/
void ConvertInt(int x, char *_a, char *_b)
{
  *_a = x & 0xff;
  *_b = (x>>8) ;
}

struct sockaddr_in si_me, si_other;
int playerSocket;

int playType = CLIENT;



#define bool int
#define true 1
#define false 0

bool done = 0;
struct Point tokenLocations[] = {
                           {2,1},{2,6}, {2,11},
                           {7,1},{7,6}, {7,11},
                           {12,1},{12,6}, {12,11},
                         };

int xLoc = 0;
int yLoc = 3;

unsigned int xSelections = 0;
unsigned int oSelections = 0;

int turn = 0;
int gameOver = 0;

void InitScreen();
int InitClientSocket();
int InitServerSocket();
void ProcessInputs();
int PlaceToken();
void EvalBoard();
void BuildMessage(char * _buf);
void RedrawBoard();
void ProcessMsg(char * buf);
void WaitForOpponent();
int connect_to_apctl(int config);
void GetPlayType();
int GetInput();
void ConvertIpToChar(int * _ipDigits, char * _ip);
void GetServerIp(char * _ip);

int exit_callback(int arg1, int arg2, void * common)
{
  sceKernelExitGame();
  return 0;

}

int CallbackThread(SceSize args, void * argp)
{
  int cbid;
  cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();
  return 0;
}

int SetupCallbacks(void)
{
  int thid = 0;
  thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 
                                0xFA0, 0, 0);
  if (thid >= 0)
  {
    sceKernelStartThread(thid, 0, 0);
  }
  return thid;
}

int main()
{
//  char serverIp[16];
  pspDebugScreenInit();
  SetupCallbacks();
  printf("Setting up client socket\n");

  GetPlayType();

  if (playType == CLIENT)
  {
    GetServerIp(SRV_IP); 
    pspDebugScreenSetXY(0,10);
    InitClientSocket();

  }
  else
  {
    printf("Server selected, not supported!\n");
    InitServerSocket();
  }
  sceKernelDelayThread(500000);
  while (!done)
  {
    gameOver = false;
    xLoc = 0;
    yLoc = 3;
    if (playType == SERVER)
      turn = 0;
    else
      turn = 1;
    xSelections = 0;
    oSelections = 0;

    InitScreen();
    while(!gameOver)
    {
      if (playType == CLIENT)
      {
        WaitForOpponent();
        EvalBoard();
        if (gameOver)
          break;
        ProcessInputs();
      }
      else
      {
        ProcessInputs();
        EvalBoard();
        if (gameOver)
          break;
        WaitForOpponent();
        EvalBoard();
      }
    }
  }
  sceKernelSleepThread();
  return 0;
}
void WaitForOpponent()
{

  int s, i, j, slen=sizeof(si_other);
  char buf[BUFLEN];

  pspDebugScreenSetXY(20,10);
  printf("Wait for opp!\n");
  if (recvfrom(playerSocket, buf, BUFLEN, 0, &si_other, &slen)==-1)
    diep("recvfrom()");
  pspDebugScreenSetXY(20,10);
  printf("            !\n");
  ProcessMsg(buf);
  if (buf[0] == PLAYERMOVE)
  {
    //ack...
    RedrawBoard();
    pspDebugScreenSetXY(19,30);
    printf("Received MOVE cmd");
  }
  else
  {
    pspDebugScreenSetXY(19,30);
    printf("Received Unknown cmd");
  }
}
/*************************************************************/
void ProcessMsg(char * buf)
{
  if (buf[0] == PLAYERMOVE)
  {
    if (buf[5] == ENDOFMESSAGE)
    {
      xSelections = BuildIt(buf[1], buf[2]);
      oSelections = BuildIt(buf[3], buf[4]);
    }
  }
}

/*************************************************************/
int InitServerSocket()
{
  int s, err, i, j, slen=sizeof(si_other);
  char buf[BUFLEN];
  printf("INITSERVERSOCKET!\n");
  if (pspSdkLoadInetModules() < 0)
  {
    printf("COULD NOT LOAD INET MODULES!\n");
    sceKernelSleepThread();
    return 0;
  }
  printf("InetModules Loaded!\n");

/*
  thid = sceKernelCreateThread("net_thread", net_thread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
  if (thid < 0)
  {
    printf("Error could not create thread!\n");
    sceKernelSleepThread();
    return 0;
  }
  sceKernelStartThread(thid, 0, NULL);
  sceKernelExitDeleteThread(0);
  sceKernelSleepThread();
*/
  if((err = pspSdkInetInit()))
  {
    printf(": Error, could not initialise the network \n", err);
  }
  printf("Now connecting to 0\n");
  if(connect_to_apctl(0))
  {
    // connected, get my IPADDR and run test
    char szMyIPAddr[32];
    printf("Success!\n");
    if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
      strcpy(szMyIPAddr, "unknown IP address");  
  }

  if ((playerSocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
   diep("socket");

  memset((char *) &si_other, sizeof(si_other), 0);
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(playerSocket, &si_me,sizeof(si_me)) == -1)
  {
    fprintf(stderr, "server bind failed\n");
    exit(1);
  }
  printf("Server Socket Up Waiting for player!\n");
  if (recvfrom(playerSocket, buf, BUFLEN,0,&si_other, &slen)==-1)
  {
    fprintf(stderr, "Server connect failed!\n");
    exit(1);
  }
  printf("Client Connected!\n");
}
/*************************************************************/
int InitClientSocket()
{
  int s, err, i, j, slen=sizeof(si_other);
  char buf[BUFLEN];
  printf("INITCLIENTSOCKET!\n");
  if (pspSdkLoadInetModules() < 0)
  {
    printf("COULD NOT LOAD INET MODULES!\n");
    sceKernelSleepThread();
    return 0;
  }
  printf("InetModules Loaded!\n");

/*
  thid = sceKernelCreateThread("net_thread", net_thread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
  if (thid < 0)
  {
    printf("Error could not create thread!\n");
    sceKernelSleepThread();
    return 0;
  }
  sceKernelStartThread(thid, 0, NULL);
  sceKernelExitDeleteThread(0);
  sceKernelSleepThread();
*/
  if((err = pspSdkInetInit()))
  {
    printf(": Error, could not initialise the network \n", err);
  }
  printf("Now connecting to 0\n");
  if(connect_to_apctl(0))
  {
    // connected, get my IPADDR and run test
    char szMyIPAddr[32];
    printf("Success!\n");
    if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
      strcpy(szMyIPAddr, "unknown IP address");  
  }

  if ((playerSocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
   diep("socket");

  memset((char *) &si_other, sizeof(si_other), 0);
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(PORT);
  if (inet_aton(SRV_IP, &si_other.sin_addr)==0)
  {
    fprintf(stderr, "inet_aton() failed\n");
    exit(1);
  }
  printf("Sending packet %d\n", i);
  sprintf(buf, "This is packet %d\n", i);
  if (sendto(playerSocket, buf, BUFLEN, 0, &si_other, slen)==-1)
    diep("sendto()");
}
int connect_to_apctl(int config)
{
  int err;
  int stateLast = -1;
  printf("Inside connect_to_ap\n");
  /* Connect using the first profile */
  err = sceNetApctlConnect(config);
  if (err != 0)
  {
    printf(": sceNetApctlConnect returns %08X\n", err);
    return 0;
  }

  printf(": Connecting...\n");
  while (1)
  {
    int state;
    err = sceNetApctlGetState(&state);
    if (err != 0)
    {
      printf(": sceNetApctlGetState returns $%x\n" , err);
      break;
    }
    if (state > stateLast)
    {
      printf("  connection state %d of 4\n", state);
      stateLast = state;
    }
    if (state == 4)
      break;  // connected with static IP

    // wait a little before polling again
    sceKernelDelayThread(50*1000); // 50ms
  }
  printf(": Connected!\n");

  if(err != 0)
  {
    return 0;
  }
  return 1;
}
void SendMove()
{
  int slen = sizeof(si_other);
  char buf[512];
  memset(buf, 0, 512);
  BuildMessage(buf);
  if (sendto(playerSocket,buf,BUFLEN,0,&si_other,slen) == -1)
    printf("Could not send packet!\n");
  printf("\x1b[19;10H");
  printf("Message Sent      ");
}
/*************************************************************/
void BuildMessage(char * _buf)
{
  _buf[0] = PLAYERMOVE;
  ConvertInt(xSelections, &_buf[1], &_buf[2]);
  ConvertInt(oSelections, &_buf[3], &_buf[4]);
  _buf[5] = ENDOFMESSAGE;
}

/*************************************************************/

void ProcessInputs()
{
  SceCtrlData pad;
  bool success = false;
  while(!success)
  {
    sceCtrlReadBufferPositive(&pad, 1);
    if (pad.Buttons !=0)
    {
      if (pad.Buttons & PSP_CTRL_SQUARE)
      {
        //Square
      }
      if (pad.Buttons & PSP_CTRL_TRIANGLE)
      {
        gameOver = true;
        success = true;
        done = true;
        //Triangle
      }
      if (pad.Buttons & PSP_CTRL_CIRCLE)
      {
        done = true;
        //Circle
      }
      if (pad.Buttons & PSP_CTRL_CROSS)
      {
        success = PlaceToken();
        //Cross
      }
      if (pad.Buttons & PSP_CTRL_UP)
      {
         pspDebugScreenSetXY(xLoc,yLoc);
         printf(" ");
         yLoc -=5;
         if (yLoc < 3)
           yLoc = 3;

        //Up
      }
      if (pad.Buttons & PSP_CTRL_DOWN)
      {
        pspDebugScreenSetXY(xLoc,yLoc);
        printf(" ");
        yLoc +=5;
        if (yLoc >13)
          yLoc = 13;
        
        //DOWN
      }
      if (pad.Buttons & PSP_CTRL_LEFT)
      {
        pspDebugScreenSetXY(xLoc,yLoc);
        printf(" ");
        xLoc -=5;
        if (xLoc < 0)
          xLoc = 0;
        //Left
      }
      if (pad.Buttons & PSP_CTRL_RIGHT)
      {
        pspDebugScreenSetXY(xLoc,yLoc);
        printf(" ");
        xLoc+=5;
        if ( xLoc > 10)
          xLoc = 10;
        //Right
      }
      if (pad.Buttons & PSP_CTRL_START)
      {
        //Start
      }
      if (pad.Buttons & PSP_CTRL_SELECT)
      {
        //Select
      }
      if (pad.Buttons & PSP_CTRL_LTRIGGER)
      {
        //LTRIGGER
      }
      if (pad.Buttons & PSP_CTRL_RTRIGGER)
      {
        //RTrigger
      }

      pspDebugScreenSetXY(xLoc,yLoc);
      printf(".");
      sceKernelDelayThread(250000);
//      sleep(1);

    }
  }

}

/*************************************************************/
void RedrawBoard()
{
  int x;
  for (x = 0; x<9; x++)
  {
    if (xSelections & 1<<x)
    {
      pspDebugScreenSetXY(tokenLocations[x].x, tokenLocations[x].y);
      printf("%c", turnChars[0]);//magic number?
    }
    else if (oSelections & 1<<x)
    {
      pspDebugScreenSetXY(tokenLocations[x].x, tokenLocations[x].y);
      printf("%c", turnChars[1]);//magic number?
    }
  }
}


/*************************************************************/
int PlaceToken()
{
  int spot = -1;

  if (xLoc == 0)
  {
    if (yLoc == 3)
    {
      spot = 0;

    }
    if (yLoc == 8)
    {
      spot = 3;

    }
    if (yLoc == 13)
    {
      spot = 6;

    }
  }
  else if (xLoc == 5)
  {
    if (yLoc == 3)
    {
      spot = 1;

    }
    if (yLoc == 8)
    {
      spot = 4;

    }
    if (yLoc == 13)
    {

      spot = 7;
    }

  }
  else if (xLoc == 10)
  {

    if (yLoc == 3)
    {
      spot = 2;

    }
    if (yLoc == 8)
    {
      spot = 5;

    }
    if (yLoc == 13)
    {
      spot = 8;

    }
  }
  if (spot > -1)
  {
    if (!(xSelections & 1<<spot) && !(oSelections & 1<<spot))
    {
      pspDebugScreenSetXY(tokenLocations[spot].x, tokenLocations[spot].y);
      printf("%c", turnChars[turn]);
      if (turn == 0)
      {
        xSelections |= 1<<spot;
      }
      else
      {
        oSelections |= 1<<spot;
      }
      EvalBoard();
      SendMove();
      return true;
    }
  }
  return false;
}
/*************************************************************/
void EvalBoard()
{
  pspDebugScreenSetXY(30,30);
  int boardState = CheckForWinner(xSelections,oSelections);
  if (boardState == -1)
    return;
  if (boardState == 2)
  {
	printf("Draw!");
	gameOver = true;

    return;
  }
  if (boardState == 0)
  {
    printf("XWINS!");
	gameOver = true;
	//XWins!

    return;
  }
  if (boardState == 1)
  {
    printf("OWINS!");
	gameOver = true;
	//yWins!
    return;
  }

}

/*************************************************************/
void InitScreen()
{
  pspDebugScreenSetXY(0,0);
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("---------------\n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("---------------\n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");
  printf("    |    |     \n");

}
/*************************************************************/
void GetPlayType()
{
  bool selectDone = false;
  int button = 0;
  int xLoc = 11;  //23
  pspDebugScreenSetXY(0,0);
  printf("Please select Play Type:\n");
  printf("Local     Client      Server\n");

  while (!selectDone)
  {
    button = GetInput();
    if (button == PSP_CTRL_CROSS)
    {
      selectDone = true;
    }
    else if (button == PSP_CTRL_RIGHT)
    {
      pspDebugScreenSetXY(xLoc,3);
      printf(" ");
      if (xLoc == 11)
        xLoc = 23;
      else
        xLoc = 11;
      pspDebugScreenSetXY(xLoc,3);
      printf("x");

    }
    else if (button == PSP_CTRL_LEFT)
    {
      pspDebugScreenSetXY(xLoc,3);
      printf(" ");
      if (xLoc == 11)
        xLoc = 23;
      else
        xLoc = 11;
      pspDebugScreenSetXY(xLoc,3);
      printf("x");
    }
  } 
  if (xLoc == 11)
    playType = CLIENT;
  else
    playType = SERVER;

}
/*************************************************************/
void GetServerIp(char * _ip)
{
  int xPos = 0;
  int x;
  bool ipDone = false;
  int button = 0;
  int ipDigits[16] ;
  for (x = 0; x< 16; x++)
    ipDigits[x] = 0;
  pspDebugScreenSetXY(0,5);
  printf("Please Enter Server Ip Address and press X when done");
  pspDebugScreenSetXY(0,6);
  printf("000.000.000.000");
  while (!ipDone)
  {
    sceKernelDelayThread(250000);
    button = GetInput();
    if (button == PSP_CTRL_CROSS)
    {
      ipDone = true;
    } 
    if (button == PSP_CTRL_UP)
    {
      ipDigits[xPos] ++;
      if (ipDigits[xPos] > 9)
        ipDigits[xPos] = 0;
      pspDebugScreenSetXY(xPos,6);
      printf("%d", ipDigits[xPos]);
    }
    if (button == PSP_CTRL_DOWN)
    {
      ipDigits[xPos] --;
      if (ipDigits[xPos] < 0)
        ipDigits[xPos] = 9;
      pspDebugScreenSetXY(xPos,6);
      printf("%d", ipDigits[xPos]);
    }
    if (button == PSP_CTRL_RIGHT)
    {
      pspDebugScreenSetXY(xPos,7);
      printf(" ");
      xPos ++;
      if (xPos == 3 || xPos == 7 || xPos == 11)
        xPos++;
      if (xPos > 14)
        xPos = 0;
      pspDebugScreenSetXY(xPos,7);
      printf("^");
    }
    if (button == PSP_CTRL_LEFT)
    {
      pspDebugScreenSetXY(xPos,7);
      printf(" ");
      xPos --;
      if (xPos == 3 || xPos == 7 || xPos == 11)
        xPos--;
      if (xPos <0 )
        xPos = 14;
      pspDebugScreenSetXY(xPos,7);
      printf("^");
    }
    ConvertIpToChar(ipDigits,_ip);    
    

  }


}
/*************************************************************/
void ConvertIpToChar(int * _ipDigits, char * _ip)
{
  int x = 0;
  int totalChars = 0;
  bool start = true;
  for (x = 0; x<15; x++)
  {
    if (x == 3 || x == 7 || x == 11)
    {
      _ip[totalChars] = '.';
      start = true;
      totalChars ++;
    }
    else if (start && _ipDigits[x] == 0)
      continue;
    else
    {
      switch (_ipDigits[x])
      {
        case (0):
        {
          _ip[totalChars] = '0';
          totalChars ++;
          break;
        }
        case (1):
        {
          _ip[totalChars] = '1';
          totalChars ++;
          break;
        }
        case (2):
        {
          _ip[totalChars] = '2';
          totalChars ++;
          break;
        }
        case (3):
        {
          _ip[totalChars] = '3';
          totalChars ++;
          break;
        }
        case (4):
        {
          _ip[totalChars] = '4';
          totalChars ++;
          break;
        }

        case (5):
        {
          _ip[totalChars] = '5';
          totalChars ++;
          break;
        }
        case (6):
        {
          _ip[totalChars] = '6';
          totalChars ++;
          break;
        }
        case (7):
        {
          _ip[totalChars] = '7';
          totalChars ++;
          break;
        }
        case (8):
        {
          _ip[totalChars] = '8';
          totalChars ++;
          break;
        }
        case (9):
        {
          _ip[totalChars] = '9';
          totalChars ++;
          break;
        }
      }
      start = false;
    } 
  }
  _ip[totalChars] = '\0';



}
/*************************************************************/
int GetInput()
{
  SceCtrlData pad;
  bool success = false;
  while(!success)
  {
    sceCtrlReadBufferPositive(&pad, 1);
    if (pad.Buttons !=0)
    {
      if (pad.Buttons & PSP_CTRL_SQUARE)
      {
        success = true;
        return PSP_CTRL_SQUARE;
        //Square
      }
      if (pad.Buttons & PSP_CTRL_TRIANGLE)
      {
        success = true;
        return PSP_CTRL_TRIANGLE;
        //Triangle
      }
      if (pad.Buttons & PSP_CTRL_CIRCLE)
      {
        success = true;
        return PSP_CTRL_CIRCLE;
        //Circle
      }
      if (pad.Buttons & PSP_CTRL_CROSS)
      {
        success = true;
        return PSP_CTRL_CROSS;
        //Cross
      }
      if (pad.Buttons & PSP_CTRL_UP)
      {
        success = true;
        return PSP_CTRL_UP;
        //Up
      }
      if (pad.Buttons & PSP_CTRL_DOWN)
      {
        success = true;
        return PSP_CTRL_DOWN;
        //DOWN
      }
      if (pad.Buttons & PSP_CTRL_LEFT)
      {
        success = true;
        return PSP_CTRL_LEFT;
        //Left
      }
      if (pad.Buttons & PSP_CTRL_RIGHT)
      {
        success = true;
        return PSP_CTRL_RIGHT;
        //Right
      }
      if (pad.Buttons & PSP_CTRL_START)
      {
        success = true;
        return PSP_CTRL_SQUARE;
        //Start
      }
      if (pad.Buttons & PSP_CTRL_SELECT)
      {
        success = true;
        return PSP_CTRL_SELECT;
        //Select
      }
      if (pad.Buttons & PSP_CTRL_LTRIGGER)
      {
        success = true;
        return PSP_CTRL_LTRIGGER;
        //LTRIGGER
      }
      if (pad.Buttons & PSP_CTRL_RTRIGGER)
      {
        success = true;
        return PSP_CTRL_RTRIGGER;
        //RTrigger
      }
      sceKernelDelayThread(250000);
    }
  }
}
