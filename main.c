#include <stdio.h> //Basic input output streams
#include <ctype.h> // Character Library
#include <stdlib.h> //For exit and all
#include <termios.h> // Interactor with terminal
#include <unistd.h> // For STDIN_FILENO
#include <errno.h> // Error handling
#include <sys/ioctl.h> // To get size of terminal
                       // using TIOCGWINSZ which stands for
                       // Terminal IOCtl Get WINdow SiZe
#include <string.h>
#include "utility_functions.h"
/*
|**********************************************************|
|                       DEFINATION                         |
|**********************************************************|
*/

#define VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}


/*
|**********************************************************|
|                       DATA                               |
|**********************************************************|
*/


struct editorConfig{
    int cx,cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;
/*
|**********************************************************|
|                       TERMINAL                           |
|**********************************************************|
*/
struct abuf{
    char *b;
    int len;

};
//PROTOTYPES
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);
void editorInit();
void die(const char*);
void disableRawMode();
void enableRawMode();
void editorDrawRows(struct abuf *ab);
void editorKeyProcess();
char editorKeyRead();
void editorRefreshScreen();
int getWindowSize(int*, int*);
int getCursorPosition(int*, int*);
//DEFINATIONS
void editorInit(){
    E.cx = 0;
    E.cy = 0;
    if(getWindowSize(&E.screenrows,&E.screencols) == -1)die("Window Size");
}

void die(const char* s){
    write(STDOUT_FILENO, "\x1b[2J", 4); // <esc>[2J means clear entire screen
                                        // <esc>[1J means clear upto cursor
                                        // <esc>[J means clear from cursor to end
    write(STDOUT_FILENO, "\x1b[H", 3 ); // <esc> [H means place cursor to starting
                                        // <esc> [x;yH means place cursor to at x row and y column
                                        // <esc> x >= 1 and y >= 1
    
    
    perror(s);
    exit(1);
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)die("tcsetattr");
}

void enableRawMode(){
    // struct termios raw;
    if(tcgetattr(STDIN_FILENO,&E.orig_termios) == -1)die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | CS8 | IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)die("tcsetattr");
}

char editorKeyRead(){
    int nRead;
    char c;
    while((nRead = read(STDIN_FILENO,&c,1)) != 1){
        if(nRead == -1 && errno != EAGAIN)die("read");
    }
    return c;
}

void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 1; y <= E.screenrows; ++y){
        // write(STDOUT_FILENO, "~",1);
        // Adding a welcome message to the editor
        if(y == (min(E.screenrows/3, 3))){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Welcome message --version %s", VERSION);
            if(welcomelen > E.screencols)welcomelen=E.screencols;
            int padding = max(0, (welcomelen-E.screencols)/2);
            if(padding){
                abAppend(ab,"~",1);
                padding--;
            }
            while(padding--)abAppend(ab," ",1);
            abAppend(ab,welcome,welcomelen);
        }else{
            abAppend(ab,"~",1);
        }        
        abAppend( ab, "\x1b[K",3);
        if(y <  E.screenrows){
            abAppend(ab,"\r\n",2);
        }
    }
}

void editorKeyProcess(){
    char c = editorKeyRead();
    switch(c){
        case CTRL_KEY('q'): 
                            write(STDOUT_FILENO, "\x1b[2J", 4); // <esc>[2J means clear entire screen
                                                                // <esc>[1J means clear upto cursor
                                                                // <esc>[J means clear from cursor to end
                            write(STDOUT_FILENO, "\x1b[H", 3 ); // <esc> [H means place cursor to starting
                                                                // <esc> [x;yH means place cursor to at x row and y column
                                                                // <esc> x >= 1 and y >= 1
                            exit(0);
                            break;
    }
}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l",6); // Hiding the cursor during refresh
    // abAppend( &ab, "\x1b[2J", 4); // <esc>[2J means clear entire screen
                                  // <esc>[1J means clear upto cursor
                                  // <esc>[J means clear from cursor to end
    abAppend( &ab, "\x1b[H", 3 ); // <esc> [H means place cursor to starting
                                  // <esc> [x;yH means place cursor to at x row and y column
                                  // <esc> x >= 1 and y >= 1
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cx+1, E.cy+1);
    abAppend(&ab, buf, strlen(buf));
    // abAppend(&ab, "\x1b[H", 3 );
    abAppend(&ab, "\x1b[?25h",6); // Showing cursor ag  ain after refresh
    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12)return -1;
        return getCursorPosition(rows,cols);
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0;
    if(write(STDOUT_FILENO,"\x1b[6n",4) != 4)return -1;
    while(i < sizeof(buf)-1){
        if(read(STDIN_FILENO,&buf[i],1) != 1)break;
        if(buf[i] == 'R')break;
        ++i;
    }
    buf[i] = '\0';
    if(buf[0] != '\x1b' || buf[1] != '[')return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols)!=2)return -1;
    return 0;
}

/*
|**********************************************************|
|                  APPEND BUFFER                           |
|**********************************************************|
*/

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len+len);
    if(new == NULL)return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

/*
|**********************************************************|
|                       INIT                               |
|**********************************************************|
*/

int main(){
    enableRawMode();
    editorInit();
    char c;
    while(1){
        editorKeyProcess();
        editorRefreshScreen();
    }
    return 0;
}