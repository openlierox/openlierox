/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 * 
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Switch to gets() if $TERM is something we can't support.
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - Completion?
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * The following are used to clear the screen: ESC [ H ESC [ 2 J
 * This is actually composed of two sequences:
 *
 * cursorhome
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED2 (Clear entire screen)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 * 
 */

#include <list>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "linenoise.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static const char *unsupported_term[] = {"dumb","cons25",NULL};
static LinenoiseCompletionCallback *completionCallback = NULL;

static struct termios orig_termios; /* in order to restore at exit */
static int rawmode = 0; /* for atexit() function to check if restore is needed*/
static int rawmode_everused = 0; /* whether orig_termios was ever captured */
static int terminal_finalized = 0; /* once set, never go raw again (shutdown) */
static int atexit_registered = 0; /* register atexit just 1 time */
static size_t history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static std::vector<std::string> history;

static void linenoiseAtExit();
int linenoiseHistoryAdd(const std::string& line);

bool linenoiseIsUnsupportedTerm() {
    char *term = getenv("TERM");

    if (term == NULL) return 0;
	for (int j = 0; unsupported_term[j]; j++)
		if (!strcasecmp(term,unsupported_term[j])) return true;
	return false;
}

int linenoiseEnableRawMode(int fd) {
    struct termios raw;

	if (terminal_finalized) goto fatal; /* shutdown: never re-raw */
	if (!isatty(fd)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
	if (!rawmode) {
		if(tcgetattr(fd,&orig_termios) == -1) goto fatal;
		// Reset some flags. When OLX crashed earlier or so or some
		// other tool left the terminal in some strange state,
		// just overwrite them with some sane defaults.
		orig_termios.c_iflag |= BRKINT | ICRNL | INPCK | ISTRIP | IXON;
		orig_termios.c_oflag |= OPOST;
		orig_termios.c_lflag |= ECHO | ICANON | IEXTEN | ISIG;
		rawmode_everused = 1;
	}

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

void linenoiseDisableRawMode(int fd) {
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* Shutdown: restore cooked mode unconditionally,
 * since the rawmode flag can desync across the CLI and tee threads,
 * and latch raw off so nothing re-raws. */
void linenoiseRestoreTerminal(int fd) {
    terminal_finalized = 1;
    if (rawmode_everused && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void) {
	linenoiseDisableRawMode(STDIN_FILENO);
}

static int getColumns() {
    struct winsize ws;

	// A pty with no window size set reports ws_col == 0.
	// refreshLine() below divides the line against cols and would spin forever
	// on a zero (or absurdly small) width, so fall back to a sane default.
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return 80;
    return ws.ws_col;
}

void LinenoiseEnv::refreshLine() {
	const char* cbuf = buf.c_str();
	size_t len = buf.size();

    char seq[64];
    
	// pos and len are unsigned; guard against underflow so a small cols
	// cannot turn these into endless loops.
	while(pos > 0 && prompt.size() + pos >= cols) {
		cbuf++;
        len--;
        pos--;
    }
	while(len > 0 && prompt.size() + len > cols) {
        len--;
    }

    /* Cursor to left edge */
    snprintf(seq,64,"\x1b[0G");
	if (write(seq,strlen(seq)) == -1) return;
    /* Write the prompt and the current buffer content */
	if (write(prompt.c_str(),prompt.size()) == -1) return;
	if (write(cbuf,len) == -1) return;
    /* Erase to right */
    snprintf(seq,64,"\x1b[0K");
	if (write(seq,strlen(seq)) == -1) return;
    /* Move cursor to original position. */
	snprintf(seq,64,"\x1b[0G\x1b[%dC", (int)(pos+prompt.size()));
	if (write(seq,strlen(seq)) == -1) return;
}

void LinenoiseEnv::eraseLine() {
	char seq[64];
	/* Cursor to left edge */
	snprintf(seq,64,"\x1b[0G");
	if (write(seq,strlen(seq)) == -1) return;
	/* Erase to right */
	snprintf(seq,64,"\x1b[0K");
	if (write(seq,strlen(seq)) == -1) return;
}

static void beep() {
    fprintf(stderr, "\x7");
    fflush(stderr);
}

int LinenoiseEnv::completeLine() {
	LinenoiseCompletions lc;
	char c = 0;

	if(completionCallback(buf, &lc)) {
		// if we got true, it means that the completion-callback already has handled it
		return c;
	}

	if (lc.size() == 0) {
        beep();
    } else {
		size_t i = 0;
		bool stop = false;

        while(!stop) {
            /* Show completion or original buffer */
			if (i < lc.size()) {
				std::string origBuf(lc[i]);
				size_t origPos = lc[i].size();
				std::swap(origBuf, buf);
				std::swap(origPos, pos);
				refreshLine();
				std::swap(origBuf, buf);
				std::swap(origPos, pos);
			} else {
				refreshLine();
            }

			int nread = read(&c,1);
            if (nread <= 0) {
                return -1;
            }

            switch(c) {
                case 9: /* tab */
					i = (i+1) % (lc.size()+1);
					if (i == lc.size()) beep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
					if (i < lc.size()) {
						refreshLine();
                    }
					stop = true;
                    break;
                default:
                    /* Update buffer and return */
					if (i < lc.size()) {
						buf = lc[i];
						pos = buf.size();
                    }
					stop = true;
                    break;
            }
        }
    }

    return c; /* Return last read character */
}

void LinenoiseEnv::clearScreen() {
	if (write("\x1b[H\x1b[2J",7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
}

std::string LinenoiseEnv::getNextInput() {
	struct RawInputScope {
		LinenoiseEnv& l;
		bool v;
		RawInputScope(LinenoiseEnv& l_) : l(l_) {
			v = linenoiseEnableRawMode(l.fd) != -1;
		}
		operator bool() { return v; }
		~RawInputScope() {
			if(!v) return;
			l.eraseLine();
			linenoiseDisableRawMode(l.fd);
			printf("%s%s\n", l.prompt.c_str(), l.buf.c_str());
		}
	};
	RawInputScope rawInputScope(*this);
	if(!rawInputScope) {
		hadReadError = true;
		return "";
	}

	buf = "";
	pos = 0;
	cols = getColumns();

	std::string savedBuf;
	int history_index = -1; // -1 is savedBuf
    
	if (write(prompt.c_str(),prompt.size()) == -1) return "";
    while(1) {
        char c;
        char seq[2], seq2[2];

		int nread = read(&c,1);
		if (nread <= 0) return "";

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && completionCallback != NULL) {
			c = completeLine();
            /* Return on errors */
			if (c < 0) return "";
            /* Read next character when 0 */
            if (c == 0) continue;
        }

        switch(c) {
        case 13:    /* enter */
			linenoiseHistoryAdd(buf);
			return buf;
        case 3:     /* ctrl-c */
            errno = EAGAIN;
			return "";
        case 127:   /* backspace */
        case 8:     /* ctrl-h */
			if (pos > 0 && buf.size() > 0) {
				pos--;
				buf.erase(pos, 1);
				refreshLine();
            }
            break;
        case 4:     /* ctrl-d, remove char at right of cursor */
			if (buf.size() > 1 && pos < (buf.size()-1)) {
				buf.erase(pos, 1);
				refreshLine();
			} else if (buf.size() == 0) {
				return "";
            }
            break;
        case 20:    /* ctrl-t */
			if (pos > 0 && pos < buf.size()) {
				std::swap(buf[pos-1], buf[pos]);
				if (pos != buf.size()-1) pos++;
				refreshLine();
            }
            break;
        case 2:     /* ctrl-b */
            goto left_arrow;
        case 6:     /* ctrl-f */
            goto right_arrow;
        case 16:    /* ctrl-p */
            seq[1] = 65;
            goto up_down_arrow;
        case 14:    /* ctrl-n */
            seq[1] = 66;
            goto up_down_arrow;
            break;
        case 27:    /* escape sequence */
			if (read(seq,2) == -1) break;
            if (seq[0] == 91 && seq[1] == 68) {
left_arrow:
                /* left arrow */
                if (pos > 0) {
                    pos--;
					refreshLine();
                }
            } else if (seq[0] == 91 && seq[1] == 67) {
right_arrow:
                /* right arrow */
				if (pos != buf.size()) {
                    pos++;
					refreshLine();
                }
            } else if (seq[0] == 91 && (seq[1] == 65 || seq[1] == 66)) {
up_down_arrow:
                /* up and down arrow: history */
				if (history.size() > 0) {
					if(history_index == -1)
						savedBuf = buf;
					/* Show the new entry */
                    history_index += (seq[1] == 65) ? 1 : -1;
					if (history_index < -1) {
						history_index = -1;
                        break;
					} else if (history_index > 0 && (size_t)history_index >= history.size()) {
						history_index = history.size() - 1;
                        break;
                    }
					if(history_index == -1)
						buf = savedBuf;
					else
						buf = history[history.size()-1-history_index];
					pos = buf.size();
					refreshLine();
                }
            } else if (seq[0] == 91 && seq[1] > 48 && seq[1] < 55) {
                /* extended escape */
				if (read(seq2,2) == -1) break;
                if (seq[1] == 51 && seq2[0] == 126) {
                    /* delete */
					if (buf.size() > 0 && pos < buf.size()) {
						buf.erase(pos, 1);
						refreshLine();
                    }
                }
            }
            break;
        default:
			if (buf.size() == pos) {
				buf += (char)c;
				pos++;
				if (prompt.size()+buf.size() < cols) {
					/* Avoid a full update of the line in the
						 * trivial case. */
					if (write(&c,1) == -1) return "";
				} else {
					refreshLine();
				}
			} else {
				buf = buf.substr(0, pos) + std::string(&c, 1) + buf.substr(pos);
				pos++;
				refreshLine();
			}
			break;
        case 21: /* Ctrl+u, delete the whole line. */
			buf = "";
			pos = 0;
			refreshLine();
            break;
        case 11: /* Ctrl+k, delete from current to end of line. */
			buf.erase(pos);
			refreshLine();
            break;
        case 1: /* Ctrl+a, go to the start of the line */
            pos = 0;
			refreshLine();
            break;
        case 5: /* ctrl+e, go to the end of the line */
			pos = buf.size();
			refreshLine();
            break;
        case 12: /* ctrl+l, clear screen */
			clearScreen();
			refreshLine();
        }
    }
	return buf;
}

LinenoiseEnv::LinenoiseEnv() {
	fd = STDIN_FILENO;
	hadReadError = false;
	pos = 0;
	cols = getColumns();
}

ssize_t LinenoiseEnv::read(void* d, size_t nbyte) {
	ssize_t ret = ::read(fd, d, nbyte);
	if(ret <= 0) hadReadError = true;
	return ret;
}
ssize_t LinenoiseEnv::write(const void* d, size_t nbyte) { return ::write(fd, d, nbyte); }

std::string linenoise(const std::string& prompt) {
	if (linenoiseIsUnsupportedTerm() || !isatty(STDIN_FILENO)) {
		printf("%s",prompt.c_str());
        fflush(stdout);		

		char buf[LINENOISE_MAX_LINE];
		if (fgets(buf,LINENOISE_MAX_LINE,stdin) == NULL) return NULL;
		size_t len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
		return std::string(buf, len);

	} else { // supported term
		LinenoiseEnv l;
		l.prompt = prompt;
		return l.getNextInput();
    }
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(LinenoiseCompletionCallback *fn) {
    completionCallback = fn;
}

/* Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const std::string& line) {
    if (history_max_len == 0) return 0;
	if(line.empty()) return 0;
	if(!history.empty() && history.back() == line) return 0;
	history.push_back(line);
	if(history.size() > history_max_len)
		history.erase(history.begin(), history.begin() + history.size() - history_max_len);
	return 1;
}

int linenoiseHistorySetMaxLen(int len) {
	if (len < 1) return 0;
	history_max_len = len;
	if(history.size() > history_max_len)
		history.erase(history.begin(), history.begin() + history.size() - history_max_len);
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const std::string& filename) {
	FILE *fp = fopen(filename.c_str(), "w");
    
    if (fp == NULL) return -1;
	for (size_t j = 0; j < history.size(); j++)
		fprintf(fp,"%s\n",history[j].c_str());
    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const std::string& filename) {
	FILE *fp = fopen(filename.c_str(), "r");
    char buf[LINENOISE_MAX_LINE];
    
    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
		char* p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}
