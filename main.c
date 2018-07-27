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

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY
};
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
int editorKeyRead();
void editorRefreshScreen();
int getWindowSize(int*, int*);
int getCursorPosition(int*, int*);
void editorMovecursor(int key);
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

int editorKeyRead(){
    int nRead;
    char c;
    while((nRead = read(STDIN_FILENO,&c,1)) != 1){
        if(nRead == -1 && errno != EAGAIN)die("read");
    }
    if(c == '\x1b'){
        char seq[5];
        if(read(STDIN_FILENO,&seq[0],1)!=1)return '\x1b';
        if(read(STDIN_FILENO,&seq[1],1)!=1)return '\x1b';
        /*
        HOME_KEY and END_KEY are the real  issue
        Note: While Following are definite input of keys
        ARROW_UP = <esc>[A
        ARROW_DOWN = <esc>[B
        ARROW_RIGHT = <esc>[C
        ARROW_LEFT = <esc>[D
        PAGE_UP = <esc>[5~
        PAGE_DOWN = <esc>[6~
        but 
        HOME_KEY = <esc>[1~ | <esc>[7~ | <esc>[H | <esc>[OH  
        END_KEY =  <esc>[4~ | <esc>[8~ | <esc>[F | <esc>[OF 
        */
        if(seq[0] == '['){
            if(seq[1] >= '0' && seq[1] <= '9'){
                if((read(STDIN_FILENO,&seq[2],1) != 1))return '\x1b';
                if(seq[2] == '~'){
                    switch(seq[1]){
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '1':
                        case '7': return HOME_KEY;
                        case '4':
                        case '8': return END_KEY;
                    }
                }
            }
            else{
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    case 'O':
                        if((read(STDIN_FILENO, &seq[3],1))!=1)return '\x1b';
                        switch(seq[3]){
                            case 'H': return HOME_KEY;
                            case 'F': return END_KEY;
                        }
                }
            }
        }
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
            int padding =  (E.screencols - welcomelen)/2;
            if(padding){
                abAppend(ab,"~",1);
            }
            char str[50];
            // snprintf(str,sizeof(str),"%d",padding);
            for(int i = 1; i < padding; ++i)abAppend(ab," ",1);
            // abAppend(ab,str,strlen(str));
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
    int c = editorKeyRead();
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
        case ARROW_DOWN:
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_RIGHT:
                editorMovecursor(c);
                break;
        case PAGE_DOWN:
                E.cy = E.screenrows-1;
                break;
        case PAGE_UP: 
                E.cy = 0;
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
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+1, E.cx+1);
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

void editorMovecursor(int key){
    switch(key){
        case ARROW_LEFT:
            if(E.cx != 0)E.cx--;
            else{
                if(E.cy != 0)E.cy--;
                else E.cy = E.screenrows-1;
                
                E.cx = E.screencols-1;
            }
            break;
        case ARROW_DOWN:
            if(E.cy != E.screenrows-1)E.cy++;
            else{
                E.cy = 0;
            }
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screencols-1)E.cx++;
            else{
                if(E.cy != E.screenrows-1)E.cy++;
                else E.cy = 0;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0)E.cy--;
            else{
                E.cy = E.screenrows-1;
            }break;
    }

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