#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* Impostor 3DS — native Old 3DS homebrew prototype.
 * Based on the mechanics/map idea in the supplied HTML, rewritten for libctru.
 * Black radial fog is intentional and remains enabled during play.
 */
#define SW 400
#define SH 240
#define MW 56
#define MH 32
#define TILE 16
#define PLAYERS 8
#define TASKS 14
#define ROOM_COUNT 14
#define WORLD_W (MW*TILE)
#define WORLD_H (MH*TILE)

typedef unsigned int u32c;
typedef struct { int x1,y1,x2,y2; const char *name; } Room;
typedef struct { int x,y; } Point;
typedef struct {
    float x,y;
    int color, alive, imp, body, facing, task, aiTimer, killCooldown, reportCooldown;
    int seenKiller, vote;
} Player;

enum { MENU, PLAY, MEETING, RESULT, ENDING };
static int state = MENU, roleChoice = 2, frame = 0;
static int map[MW*MH], taskDone[TASKS], taskCount = 0, taskTarget = 14;
static int emergencyCooldown = 0, killCooldown = 0, selectedVote = 1;
static int bodyIndex = -1, meetingTimer = 0, resultTimer = 0, resultPlayer = -1;
static int winner = 0, taskProgress = 0, justVoted = 0;
static char statusText[96] = "";
static Player P[PLAYERS];
static u8 *fb = NULL;
static PrintConsole bottomConsole;

static const u32c COLORS[PLAYERS] = {
    0xE64B4B,0x2F75E8,0x25B86B,0xE6D748,0xE85EA8,0xE88936,0x36C9CE,0x9A60D9
};
static const char *NAMES[PLAYERS] = {"VOCE","AZUL","VERDE","AMARELO","ROSA","LARANJA","CIANO","ROXO"};
static const Room ROOMS[ROOM_COUNT] = {
 {2,9,8,20,"REATOR"},{9,4,14,11,"MOTOR SUP"},{9,19,14,26,"MOTOR INF"},
 {16,13,18,18,"SEGURANCA"},{20,6,26,13,"ENFERMARIA"},{26,1,37,11,"REFEITORIO"},
 {20,17,26,22,"ELETRICA"},{27,19,35,28,"ARMAZEM"},{37,16,44,22,"ADMIN"},
 {36,25,40,29,"COMUNICACOES"},{42,24,47,29,"ESCUDOS"},{49,14,54,21,"NAVEGACAO"},
 {40,10,44,14,"O2"},{41,3,47,9,"ARMAS"}
};
static const int CORRIDORS[][4] = {
 {15,7,19,8},{12,12,13,18},{14,15,15,16},{15,21,19,22},{15,25,26,26},
 {22,14,23,16},{30,12,31,18},{37,12,38,15},{38,5,40,6},{45,12,50,13},
 {45,17,48,18},{35,19,37,21},{40,26,42,27},{48,21,49,26}
};
static const Point TASK_POINTS[TASKS] = {
 {4,14},{11,6},{11,23},{17,16},{23,8},{31,9},{22,20},
 {31,24},{40,19},{38,27},{44,27},{51,17},{42,12},{44,6}
};
static const Point EMERGENCY = {32*16,5*16+8};

/* RGB888 software renderer. The 3DS framebuffer is stored column-major and vertically flipped. */
static inline void pixel(int x, int y, u32c rgb) {
    if (x < 0 || x >= SW || y < 0 || y >= SH || !fb) return;
    unsigned int off = ((unsigned int)x * SH + (SH - 1 - (unsigned int)y)) * 3;
    fb[off] = (u8)(rgb & 255);       /* B */
    fb[off+1] = (u8)((rgb >> 8) & 255); /* G */
    fb[off+2] = (u8)((rgb >> 16) & 255);/* R */
}
static void rect(int x,int y,int w,int h,u32c c) {
    int x0=x<0?0:x, y0=y<0?0:y, x1=x+w>SW?SW:x+w, y1=y+h>SH?SH:y+h;
    for(int yy=y0; yy<y1; yy++) for(int xx=x0; xx<x1; xx++) pixel(xx,yy,c);
}
static void circle(int cx,int cy,int r,u32c c) {
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++) if(x*x+y*y<=r*r) pixel(cx+x,cy+y,c);
}
static u32c shade(u32c c,int factor) {
    unsigned int r=(c>>16)&255, g=(c>>8)&255, b=c&255;
    r=r*factor/255; g=g*factor/255; b=b*factor/255;
    return (r<<16)|(g<<8)|b;
}
static const unsigned char *glyph(char c) {
    static const unsigned char blank[7]={0,0,0,0,0,0,0};
    static const unsigned char A[7]={14,17,17,31,17,17,17},B[7]={30,17,17,30,17,17,30},C[7]={14,17,16,16,16,17,14};
    static const unsigned char D[7]={30,17,17,17,17,17,30},E[7]={31,16,16,30,16,16,31},F[7]={31,16,16,30,16,16,16};
    static const unsigned char G[7]={14,17,16,23,17,17,15},H[7]={17,17,17,31,17,17,17},I[7]={14,4,4,4,4,4,14};
    static const unsigned char J[7]={7,2,2,2,18,18,12},K[7]={17,18,20,24,20,18,17},L[7]={16,16,16,16,16,16,31};
    static const unsigned char M[7]={17,27,21,21,17,17,17},N[7]={17,25,21,19,17,17,17},O[7]={14,17,17,17,17,17,14};
    static const unsigned char P[7]={30,17,17,30,16,16,16},Q[7]={14,17,17,17,21,18,13},R[7]={30,17,17,30,20,18,17};
    static const unsigned char S[7]={15,16,16,14,1,1,30},T[7]={31,4,4,4,4,4,4},U[7]={17,17,17,17,17,17,14};
    static const unsigned char V[7]={17,17,17,17,17,10,4},W[7]={17,17,17,21,21,21,10},X[7]={17,17,10,4,10,17,17};
    static const unsigned char Y[7]={17,17,10,4,4,4,4},Z[7]={31,1,2,4,8,16,31};
    static const unsigned char d0[7]={14,17,19,21,25,17,14},d1[7]={4,12,4,4,4,4,14},d2[7]={14,17,1,2,4,8,31};
    static const unsigned char d3[7]={30,1,1,14,1,1,30},d4[7]={2,6,10,18,31,2,2},d5[7]={31,16,16,30,1,1,30};
    static const unsigned char d6[7]={14,16,16,30,17,17,14},d7[7]={31,1,2,4,8,8,8},d8[7]={14,17,17,14,17,17,14},d9[7]={14,17,17,15,1,1,14};
    static const unsigned char dash[7]={0,0,0,31,0,0,0}, colon[7]={0,4,4,0,4,4,0}, percent[7]={17,2,4,8,17,0,0};
    static const unsigned char bang[7]={4,4,4,4,4,0,4}, ques[7]={14,17,1,2,4,0,4}, period[7]={0,0,0,0,0,6,6};
    static const unsigned char slash[7]={1,2,2,4,8,8,16}, plus[7]={0,4,4,31,4,4,0};
    switch(toupper((unsigned char)c)) {
      case 'A':return A;case 'B':return B;case 'C':return C;case 'D':return D;case 'E':return E;case 'F':return F;case 'G':return G;case 'H':return H;case 'I':return I;case 'J':return J;case 'K':return K;case 'L':return L;case 'M':return M;case 'N':return N;case 'O':return O;case 'P':return P;case 'Q':return Q;case 'R':return R;case 'S':return S;case 'T':return T;case 'U':return U;case 'V':return V;case 'W':return W;case 'X':return X;case 'Y':return Y;case 'Z':return Z;
      case '0':return d0;case '1':return d1;case '2':return d2;case '3':return d3;case '4':return d4;case '5':return d5;case '6':return d6;case '7':return d7;case '8':return d8;case '9':return d9;case '-':return dash;case ':':return colon;case '%':return percent;case '!':return bang;case '?':return ques;case '.':return period;case '/':return slash;case '+':return plus;case ' ':return blank;default:return blank;
    }
}
static void text(int x,int y,const char *s,u32c c,int scale) {
    for(;*s;s++,x+=6*scale) {
        const unsigned char *p=glyph(*s);
        for(int gy=0;gy<7;gy++) for(int gx=0;gx<5;gx++) if(p[gy] & (1<<(4-gx))) rect(x+gx*scale,y+gy*scale,scale,scale,c);
    }
}
static int clampi(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}
static float dist(float x1,float y1,float x2,float y2){float x=x1-x2,y=y1-y2;return sqrtf(x*x+y*y);}
static int cell(int x,int y){if(x<0||x>=MW||y<0||y>=MH)return 0;return map[y*MW+x];}
static int walkable(float x,float y) {
    int x1=(int)(x-5),x2=(int)(x+5),y1=(int)(y-5),y2=(int)(y+5);
    return cell(x1/TILE,y1/TILE)&&cell(x2/TILE,y1/TILE)&&cell(x1/TILE,y2/TILE)&&cell(x2/TILE,y2/TILE);
}
static void movePlayer(Player *p,float dx,float dy) {
    if(walkable(p->x+dx,p->y))p->x+=dx;
    if(walkable(p->x,p->y+dy))p->y+=dy;
    if(dx<-.1f)p->facing=-1;else if(dx>.1f)p->facing=1;
}
static void carve(int x1,int y1,int x2,int y2) {
    for(int y=y1;y<=y2;y++)for(int x=x1;x<=x2;x++)if(x>=0&&x<MW&&y>=0&&y<MH)map[y*MW+x]=1;
}
static void makeMap(void) {
    memset(map,0,sizeof(map));
    for(int i=0;i<ROOM_COUNT;i++)carve(ROOMS[i].x1,ROOMS[i].y1,ROOMS[i].x2,ROOMS[i].y2);
    for(unsigned int i=0;i<sizeof(CORRIDORS)/sizeof(CORRIDORS[0]);i++)carve(CORRIDORS[i][0],CORRIDORS[i][1],CORRIDORS[i][2],CORRIDORS[i][3]);
    for(int y=4;y<=6;y++)for(int x=30;x<=33;x++)map[y*MW+x]=0;
    /* Keep a small open connector through the cafeteria to the meeting console. */
    carve(30,3,33,6);
}
static int roomAt(float x,float y) {
    int cx=(int)x/TILE,cy=(int)y/TILE;
    for(int i=0;i<ROOM_COUNT;i++)if(cx>=ROOMS[i].x1&&cx<=ROOMS[i].x2&&cy>=ROOMS[i].y1&&cy<=ROOMS[i].y2)return i;
    return -1;
}
static int nearestTask(float x,float y) {
    int best=-1;float bd=1e9f;
    for(int i=0;i<TASKS;i++)if(!taskDone[i]){float d=dist(x,y,TASK_POINTS[i].x*TILE+8,TASK_POINTS[i].y*TILE+8);if(d<bd){bd=d;best=i;}}
    return best;
}
static int nextCell(float sx,float sy,float tx,float ty) {
    int start=((int)sy/TILE)*MW+(int)sx/TILE,goal=((int)ty/TILE)*MW+(int)tx/TILE;
    if(start<0||start>=MW*MH||goal<0||goal>=MW*MH||start==goal)return -1;
    static int prev[MW*MH],queue[MW*MH];
    for(int i=0;i<MW*MH;i++)prev[i]=-2;
    int head=0,tail=0;queue[tail++]=start;prev[start]=-1;
    const int off[4]={1,-1,MW,-MW};
    while(head<tail){int n=queue[head++];if(n==goal)break;for(int k=0;k<4;k++){int z=n+off[k];if(z<0||z>=MW*MH)continue;if((k==0&&n%MW==MW-1)||(k==1&&n%MW==0))continue;if(!map[z]||prev[z]!=-2)continue;prev[z]=n;queue[tail++]=z;}}
    if(prev[goal]==-2)return -1;
    int n=goal;while(prev[n]!=start&&prev[n]>=0)n=prev[n];return n;
}
static void spawnPlayers(void) {
    for(int i=0;i<PLAYERS;i++){
        P[i].x=(27+(i%4)*3)*TILE+8;P[i].y=(3+(i/4)*5)*TILE+8;
        P[i].alive=1;P[i].imp=0;P[i].body=0;P[i].facing=1;P[i].task=-1;P[i].aiTimer=0;
        P[i].killCooldown=150+rand()%120;P[i].reportCooldown=0;P[i].seenKiller=-1;P[i].vote=-1;
    }
}
static int crewAlive(void){int n=0;for(int i=0;i<PLAYERS;i++)if(P[i].alive&&!P[i].imp)n++;return n;}
static int impAlive(void){int n=0;for(int i=0;i<PLAYERS;i++)if(P[i].alive&&P[i].imp)n++;return n;}
static void checkWin(void) {
    if(state!=PLAY)return;
    if(!P[0].alive){winner=0;state=ENDING;strcpy(statusText,"VOCE FOI ELIMINADO");}
    else if(!impAlive()){winner=P[0].imp?0:1;state=ENDING;strcpy(statusText,"TODOS OS IMPOSTORES FORAM PEGOS");}
    else if(taskCount>=taskTarget){winner=P[0].imp?0:1;state=ENDING;strcpy(statusText,"TAREFAS CONCLUIDAS");}
    else if(impAlive()>=crewAlive()){winner=P[0].imp?1:0;state=ENDING;strcpy(statusText,"OS IMPOSTORES DOMINARAM A NAVE");}
}
static void startGame(void) {
    makeMap();memset(taskDone,0,sizeof(taskDone));taskCount=0;taskProgress=0;emergencyCooldown=0;killCooldown=0;bodyIndex=-1;meetingTimer=0;resultTimer=0;resultPlayer=-1;justVoted=0;
    spawnPlayers();
    if(roleChoice==1){P[0].imp=1;int j=1+rand()%(PLAYERS-1);P[j].imp=1;}
    else if(roleChoice==0){int a=1+rand()%(PLAYERS-1),b;do{b=1+rand()%(PLAYERS-1);}while(a==b);P[a].imp=P[b].imp=1;}
    else {int a=rand()%PLAYERS,b;do{b=rand()%PLAYERS;}while(a==b);P[a].imp=P[b].imp=1;}
    state=PLAY;frame=0;strcpy(statusText,"ENCONTRE TAREFAS. FIQUE DE OLHO NOS OUTROS.");
}
static void killPlayer(int victim,int killer) {
    if(victim<0||victim>=PLAYERS||!P[victim].alive)return;
    P[victim].alive=0;P[victim].body=1;P[victim].seenKiller=killer;bodyIndex=victim;
    if(victim==0){strcpy(statusText,"VOCE FOI ELIMINADO");checkWin();return;}
    if(killer>=0&&killer<PLAYERS)P[killer].killCooldown=420;
    snprintf(statusText,sizeof(statusText),"CORPO DE %s ENCONTRADO",NAMES[victim]);
    checkWin();
}
static void openMeeting(int body) {
    if(state!=PLAY)return;
    state=MEETING;bodyIndex=body;meetingTimer=0;justVoted=0;selectedVote=PLAYERS;
    for(int i=0;i<PLAYERS;i++)P[i].vote=-1;
    strcpy(statusText,body>=0?"CORPO REPORTADO! QUEM E O IMPOSTOR?":"REUNIAO DE EMERGENCIA");
}
static int nearestBody(float x,float y) {
    int best=-1;float bd=28;
    for(int i=0;i<PLAYERS;i++)if(P[i].body){float d=dist(x,y,P[i].x,P[i].y);if(d<bd){bd=d;best=i;}}
    return best;
}
static int nearestCrew(float x,float y,float limit) {
    int best=-1;float bd=limit;
    for(int i=0;i<PLAYERS;i++)if(i!=0&&P[i].alive&&!P[i].imp){float d=dist(x,y,P[i].x,P[i].y);if(d<bd){bd=d;best=i;}}
    return best;
}
static int nearWitness(float x,float y,int ignore) {
    for(int i=0;i<PLAYERS;i++)if(i!=ignore&&P[i].alive&&!P[i].imp&&dist(x,y,P[i].x,P[i].y)<100)return 1;
    return 0;
}
static void doTask(void) {
    int t=nearestTask(P[0].x,P[0].y);if(t<0){strcpy(statusText,"TODAS AS TAREFAS FEITAS!");return;}
    float d=dist(P[0].x,P[0].y,TASK_POINTS[t].x*TILE+8,TASK_POINTS[t].y*TILE+8);
    if(d>24){taskProgress=0;strcpy(statusText,"CHEGUE PERTO DO ICONE AMARELO");return;}
    taskProgress++;
    snprintf(statusText,sizeof(statusText),"FAZENDO TAREFA %d/14  %d%%",t+1,taskProgress*100/35);
    if(taskProgress>=35){taskDone[t]=1;taskCount++;taskProgress=0;strcpy(statusText,"TAREFA CONCLUIDA!");checkWin();}
}
static void botUpdate(int i) {
    Player *p=&P[i];if(!p->alive||state!=PLAY)return;
    if(p->killCooldown>0)p->killCooldown--;
    if(p->reportCooldown>0)p->reportCooldown--;
    p->aiTimer--;
    if(p->imp&&p->killCooldown<=0){
        int target=-1;float best=215;
        for(int j=0;j<PLAYERS;j++)if(P[j].alive&&!P[j].imp){float d=dist(p->x,p->y,P[j].x,P[j].y);if(d<best){best=d;target=j;}}
        if(target>=0){float dx=P[target].x-p->x,dy=P[target].y-p->y,dl=sqrtf(dx*dx+dy*dy);
            if(dl<17&&!nearWitness(p->x,p->y,target)){killPlayer(target,i);p->killCooldown=480;return;}
            if(dl>1)movePlayer(p,dx/dl*1.1f,dy/dl*1.1f);
            return;
        }
    }
    if(p->aiTimer<=0||p->task<0||taskDone[p->task]){p->task=nearestTask(p->x,p->y);if(p->task<0)p->task=rand()%TASKS;p->aiTimer=50+rand()%70;}
    float tx=TASK_POINTS[p->task].x*TILE+8,ty=TASK_POINTS[p->task].y*TILE+8;
    int n=nextCell(p->x,p->y,tx,ty);
    if(n>=0){float nx=(n%MW)*TILE+8,ny=(n/MW)*TILE+8,dx=nx-p->x,dy=ny-p->y,dl=sqrtf(dx*dx+dy*dy);if(dl>1)movePlayer(p,dx/dl*0.8f,dy/dl*0.8f);}
    if(!p->imp&&dist(p->x,p->y,tx,ty)<12&&!taskDone[p->task]){if((frame+i*7)%140==0){taskDone[p->task]=1;taskCount++;checkWin();}}
    if(!p->imp&&bodyIndex>=0&&P[bodyIndex].body&&dist(p->x,p->y,P[bodyIndex].x,P[bodyIndex].y)<23&&p->reportCooldown==0&&rand()%200==0){p->reportCooldown=90;openMeeting(bodyIndex);}
}
static void drawCrewmate(int x,int y,u32c color,int facing,int dead) {
    if(dead){circle(x,y+1,7,0x000000);rect(x-7,y-4,14,7,0x777D8B);rect(x-5,y-8,10,4,0xA5AAB7);rect(x-6,y+3,5,4,0xD5D8DF);rect(x+2,y+3,5,4,0xD5D8DF);return;}
    rect(x-9,y+9,18,3,0x07090E);
    rect(x-8,y-7,16,14,shade(color,210));rect(x-6,y-12,12,6,color);
    rect(x-8,y+5,6,6,shade(color,220));rect(x+2,y+5,6,6,shade(color,220));
    if(facing>=0){rect(x-1,y-7,10,6,0xA9E7FA);rect(x+1,y-6,7,3,0xE8FAFF);}
    else{rect(x-9,y-7,10,6,0xA9E7FA);rect(x-8,y-6,7,3,0xE8FAFF);}
    rect(x-8,y-10,6,2,shade(color,255));
}
static void drawWorld(void) {
    int cx=clampi((int)P[0].x-SW/2,0,WORLD_W-SW),cy=clampi((int)P[0].y-SH/2,0,WORLD_H-SH);
    rect(0,0,SW,SH,0x080B12);
    int x0=cx/TILE,y0=cy/TILE,x1=(cx+SW)/TILE+1,y1=(cy+SH)/TILE+1;
    for(int ty=y0;ty<=y1;ty++)for(int tx=x0;tx<=x1;tx++){
        int sx=tx*TILE-cx,sy=ty*TILE-cy;
        if(cell(tx,ty)){
            rect(sx,sy,TILE,TILE,((tx+ty)&1)?0x27334B:0x2B3852);
            rect(sx,sy+TILE-2,TILE,2,0x202A3E);
            if((tx+ty)%5==0)rect(sx+3,sy+3,2,2,0x36425C);
        } else if(cell(tx+1,ty)||cell(tx-1,ty)||cell(tx,ty+1)||cell(tx,ty-1)){
            rect(sx,sy,TILE,TILE,0x5B678D);rect(sx,sy+TILE-4,TILE,4,0x434D6F);
            rect(sx,sy, TILE,2,0x7784AA);
        }
    }
    /* Room labels and task markers */
    for(int i=0;i<ROOM_COUNT;i++){int x=((ROOMS[i].x1+ROOMS[i].x2+1)*8)-cx;int y=ROOMS[i].y1*TILE+5-cy;if(x>-80&&x<SW+80&&y>-8&&y<SH)text(x,y,ROOMS[i].name,0x7C8BA8,1);}
    for(int i=0;i<TASKS;i++)if(!taskDone[i]){int x=TASK_POINTS[i].x*TILE+8-cx,y=TASK_POINTS[i].y*TILE+8-cy;rect(x-4,y-4,8,8,0xF4C542);rect(x-2,y-2,4,4,0xFFF2A2);}
    int ex=EMERGENCY.x-cx,ey=EMERGENCY.y-cy;rect(ex-5,ey-6,10,12,0x9E2525);rect(ex-3,ey-4,6,7,0xE44A4A);
    for(int i=0;i<PLAYERS;i++)if(P[i].body){drawCrewmate((int)P[i].x-cx,(int)P[i].y-cy,P[i].imp?0xB51F1F:COLORS[i],P[i].facing,1);}
    for(int i=PLAYERS-1;i>=0;i--)if(P[i].alive){int x=(int)P[i].x-cx,y=(int)P[i].y-cy;if(i==0||dist(P[i].x,P[i].y,P[0].x,P[0].y)<155){drawCrewmate(x,y,COLORS[i],P[i].facing,0);if(P[0].imp&&P[i].imp)text(x-10,y-23,NAMES[i],0xFF6C6C,1);else if(i==0)text(x-6,y-23,"VOCE",0xFFFFFF,1);}}
    /* Radial black fog: preserved, with a slightly smoother edge. */
    int px=(int)P[0].x-cx,py=(int)P[0].y-cy;
    int radius=P[0].imp?154:119, r2=radius*radius, inner=radius*43/100, inner2=inner*inner, den=r2-inner2;
    for(int y=0;y<SH;y++){
        int dy=y-py,dy2=dy*dy;
        for(int x=0;x<SW;x++){
            int dx=x-px,d2=dx*dx+dy2;if(d2<=inner2)continue;
            int a;if(d2>=r2)a=255;else{int v=(d2-inner2)*255/(den?den:1);a=(v*v)/255;}
            unsigned int off=((unsigned int)x*SH+(SH-1-(unsigned int)y))*3;
            int keep=255-a;fb[off]=(u8)(fb[off]*keep/255);fb[off+1]=(u8)(fb[off+1]*keep/255);fb[off+2]=(u8)(fb[off+2]*keep/255);
        }
    }
    /* Always-visible task strip and minimap */
    rect(8,8,138,18,0x070A10);rect(10,10,134,4,0x293449);
    rect(10,10,(132*taskCount)/taskTarget,4,0x37D67A);
    char tmp[32];snprintf(tmp,sizeof(tmp),"TAREFAS %d/%d",taskCount,taskTarget);text(10,17,tmp,0xF1F4FA,1);
    rect(329,7,63,39,0x0B0E16);rect(331,9,56,32,0x263149);
    for(int ty=0;ty<MH;ty++)for(int tx=0;tx<MW;tx++)if(cell(tx,ty))pixel(332+tx,10+ty,0x5D6C8E);
    pixel(332+(int)P[0].x/TILE,10+(int)P[0].y/TILE,0xFFFFFF);
    if(P[0].imp&&killCooldown<=0){rect(8,32,90,12,0x3C1118);text(12,35,"MATAR PRONTO",0xFF6969,1);}
    if(bodyIndex>=0&&P[bodyIndex].body){rect(8,49,116,12,0x3C1118);text(11,52,"CORPO NO MAPA",0xFF9A9A,1);}
}
static void hud(void) {
    consoleSelect(&bottomConsole);consoleClear();
    printf("IMPOSTOR 3DS  |  MODO NATIVO OLD 3DS\n");
    printf("------------------------------------\n");
    printf("PAPEL: %s\n",P[0].imp?"IMPOSTOR":"TRIPULANTE");
    printf("TAREFAS: %d/%d   TRIPULANTES: %d\n",taskCount,taskTarget,crewAlive());
    printf("IMPOSTORES VIVOS: %d\n",impAlive());
    int r=roomAt(P[0].x,P[0].y);printf("LOCAL: %s\n",r>=0?ROOMS[r].name:"CORREDOR");
    printf("\nCONTROLES\nD-PAD: mover | A: tarefa/confirmar\nY: matar (impostor) | X: reportar\nB: reuniao de emergencia / pular\nSTART: voltar ao menu apos partida\n\n%s\n",statusText);
}
static void drawMenu(void) {
    rect(0,0,SW,SH,0x070A12);
    for(int i=0;i<44;i++){int x=(i*79+frame*2)%SW,y=(i*41+frame/3)%SH;pixel(x,y,0x34415A);}
    text(78,23,"IMPOSTOR 3DS",0xF0F4FF,3);
    text(92,50,"BLACKOUT EDITION",0x39D9D0,1);
    drawCrewmate(118,96,0x2F75E8,1,0);drawCrewmate(169,90,0xE64B4B,1,0);drawCrewmate(218,96,0x25B86B,-1,0);
    rect(45,125,310,2,0x34415A);
    text(63,140,"ESCOLHA SEU PAPEL",0xE9EDF5,1);
    const char *opts[3]={"TRIPULANTE","IMPOSTOR","SURPRESA"};
    for(int i=0;i<3;i++){int x=25+i*118;rect(x,158,110,28,i==roleChoice?0x1E695F:0x222B3C);rect(x,158,110,2,i==roleChoice?0x42E3C5:0x49556C);text(x+12,169,opts[i],i==roleChoice?0xFFFFFF:0xAAB5CC,1);}
    text(44,205,"ESQ/DIR: PAPEL    A: COMECAR",0xF5D56E,1);
    text(72,222,"NEVOA PRETA ATIVA DURANTE O JOGO",0x74829A,1);
    consoleSelect(&bottomConsole);consoleClear();
    printf("IMPOSTOR 3DS - BLACKOUT EDITION\n\nAperte ESQUERDA/DIREITA para\nescolher TRIPULANTE, IMPOSTOR ou\nSURPRESA. Aperte A para iniciar.\n\nJogo solo contra 7 bots.\nTarefas, sabotagem visual, corpos,\nreunioes, votacao e vitoria/derrota.\n\nFeito para o Old Nintendo 3DS.\nA nevoa radial preta foi mantida.\n");
}
static void drawMeeting(void) {
    rect(0,0,SW,SH,0x101522);circle(200,54,44,0x55452F);circle(200,54,37,0x8A7451);
    text(105,18,"REUNIAO",0xFFFFFF,2);
    if(bodyIndex>=0&&bodyIndex<PLAYERS)text(133,40,"CORPO REPORTADO",0xFBE4B0,1);else text(125,40,"EMERGENCIA",0xFBE4B0,1);
    for(int i=0;i<PLAYERS;i++){
        int col=i%2,row=i/2,x=16+col*190,y=76+row*35;
        if(!P[i].alive){rect(x,y,177,27,0x252A35);text(x+8,y+10,"ELIMINADO",0x757F91,1);continue;}
        rect(x,y,177,27,(selectedVote==i)?0x276B60:0x252D40);
        rect(x+5,y+7,12,12,COLORS[i]);text(x+24,y+10,NAMES[i],0xFFFFFF,1);
        if(i==0)text(x+135,y+10,"VOCE",0xF5D56E,1);
    }
    rect(16,222,177,14,selectedVote==PLAYERS?0x7C4F24:0x302A24);text(22,226,"PULAR VOTO",0xF7E4C8,1);
    char t[30];snprintf(t,sizeof(t),"TEMPO %d",(15-meetingTimer/30)>0?(15-meetingTimer/30):0);text(314,226,t,0xFFFFFF,1);
    consoleSelect(&bottomConsole);consoleClear();printf("REUNIAO DE EMERGENCIA\n\nEscolha quem deve ser ejetado.\n\nD-PAD: mudar suspeito\nA: votar no selecionado\nB: pular voto\n\nBots votam automaticamente.\nNao existe chat por teclado nesta\nversao nativa; a votacao e direta.\n");
}
static void resolveMeeting(void) {
    int votes[PLAYERS+1]={0};
    for(int i=1;i<PLAYERS;i++)if(P[i].alive){
        int v=-1;
        if(bodyIndex>=0&&P[bodyIndex].seenKiller>=0&&rand()%100<42)v=P[bodyIndex].seenKiller;
        else if(rand()%100<25)v=0;
        else {int options[PLAYERS],n=0;for(int j=0;j<PLAYERS;j++)if(P[j].alive&&j!=i)options[n++]=j;if(n&&rand()%100<70)v=options[rand()%n];}
        if(v>=0&&v<PLAYERS&&P[v].alive)votes[v]++;else votes[PLAYERS]++;
    }
    if(P[0].alive){if(selectedVote==PLAYERS)votes[PLAYERS]++;else votes[selectedVote]++;}
    int best=-1,max=0,tie=0;for(int i=0;i<=PLAYERS;i++){if(votes[i]>max){max=votes[i];best=i;tie=0;}else if(votes[i]==max&&max>0)tie=1;}
    if(best>=0&&best<PLAYERS&&!tie){P[best].alive=0;P[best].body=0;resultPlayer=best;snprintf(statusText,sizeof(statusText),"%s FOI EJETADO",NAMES[best]);}
    else {resultPlayer=-1;strcpy(statusText,"NINGUEM FOI EJETADO");}
    bodyIndex=-1;for(int i=0;i<PLAYERS;i++)P[i].body=0;
    state=RESULT;resultTimer=100;
}
static void drawResult(void) {
    rect(0,0,SW,SH,0x141A29);text(78,50,"RESULTADO DA VOTACAO",0xFFFFFF,2);
    if(resultPlayer>=0){drawCrewmate(105,126,COLORS[resultPlayer],1,0);text(142,119,NAMES[resultPlayer],0xF5D56E,2);text(143,143,P[resultPlayer].imp?"ERA IMPOSTOR!":"NAO ERA IMPOSTOR",P[resultPlayer].imp?0xFF6868:0xAEBAD0,1);}
    else text(100,129,"NINGUEM FOI EJETADO",0xF5D56E,1);
    text(102,183,"A PARTIDA CONTINUA...",0x8B9AB6,1);
    consoleSelect(&bottomConsole);consoleClear();printf("RESULTADO\n\n%s\n\nA partida continua em alguns\nsegundos.\n", statusText);
}
static void drawEnding(void) {
    rect(0,0,SW,SH,0x080B12);
    text(112,35,winner?"VITORIA!":"DERROTA!",winner?0x43E28A:0xFF6969,3);
    text(29,79,statusText,0xF1F4FA,1);
    text(84,107,P[0].imp?"VOCE ERA IMPOSTOR":"VOCE ERA TRIPULANTE",0xA6B2C9,1);
    char tmp[42];snprintf(tmp,sizeof(tmp),"TAREFAS: %d/%d",taskCount,taskTarget);text(139,129,tmp,0xF1D36B,1);
    text(62,177,"APERTE A PARA JOGAR NOVAMENTE",0xFFFFFF,1);
    text(89,196,"START: VOLTAR AO MENU",0x7D8DA8,1);
    consoleSelect(&bottomConsole);consoleClear();printf("FIM DE JOGO\n\n%s\n\nA: jogar novamente\nSTART: menu inicial\n", statusText);
}
static void gameUpdate(u32 down,u32 held) {
    frame++;
    float dx=0,dy=0;if(held&KEY_LEFT)dx-=1;if(held&KEY_RIGHT)dx+=1;if(held&KEY_UP)dy-=1;if(held&KEY_DOWN)dy+=1;
    float len=sqrtf(dx*dx+dy*dy);if(len>0){dx/=len;dy/=len;movePlayer(&P[0],dx*1.8f,dy*1.8f);}
    if(killCooldown>0)killCooldown--;
    if(emergencyCooldown>0)emergencyCooldown--;
    if(P[0].imp){
        if((down&KEY_Y)&&killCooldown<=0){int t=nearestCrew(P[0].x,P[0].y,31);if(t>=0){killPlayer(t,0);killCooldown=420;}else strcpy(statusText,"CHEGUE MAIS PERTO DA VITIMA");}
    } else if(held&KEY_A)doTask();else if(taskProgress>0)taskProgress=0;
    if(down&KEY_X){int b=nearestBody(P[0].x,P[0].y);if(b>=0)openMeeting(b);else if(dist(P[0].x,P[0].y,EMERGENCY.x,EMERGENCY.y)<45&&emergencyCooldown<=0){emergencyCooldown=1800;openMeeting(-1);}else strcpy(statusText,"NAO HA CORPO OU BOTAO POR PERTO");}
    if((down&KEY_B)&&state==PLAY){if(dist(P[0].x,P[0].y,EMERGENCY.x,EMERGENCY.y)<45&&emergencyCooldown<=0){emergencyCooldown=1800;openMeeting(-1);}else{int b=nearestBody(P[0].x,P[0].y);if(b>=0)openMeeting(b);}}
    if(frame%3==0)for(int i=1;i<PLAYERS&&state==PLAY;i++)botUpdate(i);
    checkWin();
}
static void meetingUpdate(u32 down) {
    meetingTimer++;
    if(down&KEY_UP){do{selectedVote=(selectedVote+PLAYERS)% (PLAYERS+1);}while(selectedVote<PLAYERS&&!P[selectedVote].alive);}
    if(down&KEY_DOWN){do{selectedVote=(selectedVote+1)%(PLAYERS+1);}while(selectedVote<PLAYERS&&!P[selectedVote].alive);}
    if(down&KEY_B){selectedVote=PLAYERS;justVoted=1;resolveMeeting();return;}
    if((down&KEY_A)&&!justVoted){if(selectedVote<PLAYERS&&!P[selectedVote].alive)return;justVoted=1;resolveMeeting();return;}
    if(meetingTimer>450)resolveMeeting();
}
int main(void) {
    gfxInitDefault();consoleInit(GFX_BOTTOM,&bottomConsole);srand((unsigned int)osGetTime());
    makeMap();
    while(aptMainLoop()){
        hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
        fb=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL);
        if(state==MENU){frame++;if(down&KEY_LEFT)roleChoice=(roleChoice+2)%3;if(down&KEY_RIGHT)roleChoice=(roleChoice+1)%3;if(down&KEY_A)startGame();drawMenu();}
        else if(state==PLAY){gameUpdate(down,held);drawWorld();hud();if(down&KEY_START)state=MENU;}
        else if(state==MEETING){meetingUpdate(down);if(state==MEETING)drawMeeting();}
        else if(state==RESULT){drawResult();if(resultTimer--<=0){state=PLAY;checkWin();}}
        else if(state==ENDING){drawEnding();if(down&KEY_A)startGame();if(down&KEY_START)state=MENU;}
        gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
    }
    gfxExit();return 0;
}
