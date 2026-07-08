/* linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
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
 */

#ifndef __LINENOISE_H
#define __LINENOISE_H

#include <string>
#include <vector>

bool linenoiseIsUnsupportedTerm();

typedef std::vector<std::string> LinenoiseCompletions;

typedef bool(LinenoiseCompletionCallback)(const std::string&, LinenoiseCompletions *);
void linenoiseSetCompletionCallback(LinenoiseCompletionCallback *);

std::string linenoise(const std::string& prompt);
int linenoiseHistoryAdd(const std::string& line);
int linenoiseHistorySetMaxLen(int len);
int linenoiseHistorySave(const std::string& filename);
int linenoiseHistoryLoad(const std::string& filename);

int linenoiseEnableRawMode(int fd);
void linenoiseDisableRawMode(int fd);
void linenoiseRestoreTerminal(int fd);

struct LinenoiseEnv {
	int fd;
	bool hadReadError;
	size_t cols;
	std::string prompt;
	std::string buf;
	size_t pos;

	LinenoiseEnv();
	std::string getNextInput();
	int completeLine();
	void eraseLine();
	void refreshLine();
	void clearScreen();

	virtual ssize_t read(void* d, size_t nbyte);
	virtual ssize_t write(const void* d, size_t nbyte);
};

#endif /* __LINENOISE_H */
