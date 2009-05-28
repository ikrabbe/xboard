/*
 * wclipbrd.c -- Clipboard routines for WinBoard
 * $Id: wclipbrd.c,v 2.1 2003/10/27 19:21:02 mann Exp $
 *
 * Copyright 2000,2009 Free Software Foundation, Inc.
 *
 * ------------------------------------------------------------------------
 *
 * GNU XBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * GNU XBoard is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.  *
 *
 *------------------------------------------------------------------------
 ** See the file ChangeLog for a revision history.  */

#include "config.h"

#include <windows.h>   /* required for all Windows applications */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>

#include "common.h"
#include "winboard.h"
#include "frontend.h"
#include "backend.h"
#include "wclipbrd.h"

/* Imports from winboard.c */
extern HWND hwndMain;

/* File globals */
static char *copyTemp;
static char *pasteTemp;

VOID 
CopyFENToClipboard()
{
  char *fen = NULL;

  fen = PositionToFEN(currentMove,1);
  if (!fen) {
    DisplayError("Unable to convert position to FEN.", 0);
    return;
  }
  if (!CopyTextToClipboard(fen))
      DisplayError("Unable to copy FEN to clipboard.", 0);
  free(fen);
}

/* [AS] */
HGLOBAL ExportGameListAsText();

VOID CopyGameListToClipboard()
{
    HGLOBAL hMem = ExportGameListAsText();
    
    if( hMem != NULL ) {
        /* Assign memory block to clipboard */
        BOOL ok = OpenClipboard( hwndMain );

        if( ok ) {
            ok = EmptyClipboard();

            if( ok ) {
                if( hMem != SetClipboardData( CF_TEXT, hMem ) ) {
                    ok = FALSE;
                }
            }

            CloseClipboard();

            if( ! ok ) {
                GlobalFree( hMem );
            }
        }

        if( ! ok ) {
            DisplayError( "Cannot copy list to clipboard.", 0 );
        }
    }
}

VOID
CopyGameToClipboard()
{
  /* A rather cheesy hack here. Write the game to a file, then read from the
   * file into the clipboard.
   */
  char *buf = NULL;
  FILE *f;
  unsigned long size;
  size_t len;
  struct stat st;

  if (!copyTemp) {
    copyTemp = tempnam(NULL, "wbcp");
  }
  if (!copyTemp) {
      DisplayError("Cannot create temporary file name.",0);
      return;
  }
  f = fopen(copyTemp, "w");
  if (!f) {
    DisplayError("Cannot open temporary file.", 0);
    return;
  }
  if (!SaveGame(f,0,"")) { 			/* call into backend */
    DisplayError("Cannot write to temporary file.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  f = fopen(copyTemp, "rb");
  if (!f) {
    DisplayError("Cannot reopen temporary file.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  if (fstat(fileno(f), &st) < 0) {
    DisplayError("Cannot determine size of file.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  size = st.st_size;
  if (size == -1) {
    DisplayError("Cannot determine size of file.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  rewind(f);
  buf = (char*)malloc(size+1);
  if (!buf) {
    DisplayError("Cannot allocate clipboard buffer.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  len = fread(buf, sizeof(char), size, f);
  if (len == -1) {
    DisplayError("Cannot read from temporary file.", 0);
    goto copy_game_to_clipboard_cleanup;
  }
  if ((unsigned long)size != (unsigned long)len) { /* sigh */ 
      DisplayError("Error reading from temporary file.", 0);
      goto copy_game_to_clipboard_cleanup;
  }
  buf[size] = 0;
  if (!CopyTextToClipboard(buf)) {
      DisplayError("Cannot copy text to clipboard", 0);
  }

copy_game_to_clipboard_cleanup:
  if (buf) free(buf);
  if (f) fclose(f);
}


int 
CopyTextToClipboard(char *text)
{
  /* some (most?) of the error checking may be overkill, 
   * but hey, this is Windows 
   */
  HGLOBAL hGlobalMem;
  LPVOID lpGlobalMem;
  BOOL locked;
  UINT lockCount;
  DWORD err;

  hGlobalMem = GlobalAlloc(GHND, (DWORD)lstrlen(text)+1);
  if (hGlobalMem == NULL) {
    DisplayError("Unable to allocate memory for clipboard.", 0);
    return FALSE;
  }
  lpGlobalMem = GlobalLock(hGlobalMem);
  if (lpGlobalMem == NULL) {
    DisplayError("Unable to lock clipboard memory.", 0);
    GlobalFree(hGlobalMem);
    return FALSE;
  }
  lstrcpy(lpGlobalMem, text);
  if (appData.debugMode) {
    lockCount = GlobalFlags(hGlobalMem) & GMEM_LOCKCOUNT;
    fprintf(debugFP, "CopyTextToClipboard(): lock count %d\n", lockCount);
  }
  SetLastError(NO_ERROR);
  locked = GlobalUnlock(hGlobalMem);
  err = GetLastError();
  if (appData.debugMode) {
    lockCount = GlobalFlags(hGlobalMem) & GMEM_LOCKCOUNT;
    fprintf(debugFP, "CopyTextToClipboard(): lock count %d\n", lockCount);
  }
  if (!locked) {
    locked = !((err == NO_ERROR) || (err == ERROR_NOT_LOCKED));
    if (appData.debugMode) {
      fprintf(debugFP, 
	      "CopyTextToClipboard(): err %d locked %d\n", err, locked);
    }
  }
  if (locked) {
    DisplayError("Cannot unlock clipboard memory.", 0);
    GlobalFree(hGlobalMem);
    return FALSE;
  }
  if (!OpenClipboard(hwndMain)) {
    DisplayError("Cannot open clipboard.", 0);
    GlobalFree(hGlobalMem);
    return FALSE;
  }
  if (!EmptyClipboard()) {
    DisplayError("Cannot empty clipboard.", 0);
    return FALSE;
  }
  if (hGlobalMem != SetClipboardData(CF_TEXT, hGlobalMem)) {
    DisplayError("Cannot copy text to clipboard.", 0);
    CloseClipboard();
    GlobalFree(hGlobalMem);
    return FALSE;
  }
  if (!CloseClipboard())
    DisplayError("Cannot close clipboard.", 0);
  
  return TRUE;
}

/* [AS] Reworked paste functions so they can work with strings too */

VOID PasteFENFromString( char * fen )
{
  if (appData.debugMode) {
    fprintf(debugFP, "PasteFenFromString(): fen '%s'\n", fen);
  }
  EditPositionPasteFEN(fen); /* call into backend */
  free(fen);
}


VOID
PasteFENFromClipboard()
{
  char *fen = NULL;
  if (!PasteTextFromClipboard(&fen)) {
      DisplayError("Unable to paste FEN from clipboard.", 0);
      return;
  }
  PasteFENFromString( fen );
}

VOID PasteGameFromString( char * buf )
{
  FILE *f;
  size_t len;
  if (!pasteTemp) {
    pasteTemp = tempnam(NULL, "wbpt");
  }
  f = fopen(pasteTemp, "w");
  if (!f) {
    DisplayError("Unable to create temporary file.", 0);
    free(buf); /* [AS] */
    return;
  }
  len = fwrite(buf, sizeof(char), strlen(buf), f);
  fclose(f);
  if (len != strlen(buf)) {
    DisplayError("Error writing to temporary file.", 0);
    free(buf); /* [AS] */
    return;
  }
  LoadGameFromFile(pasteTemp, 0, "Clipboard", TRUE);
  free( buf ); /* [AS] */
}


VOID
PasteGameFromClipboard()
{
  /* Write the clipboard to a temp file, then let LoadGameFromFile()
   * do all the work.  */
  char *buf;
  if (!PasteTextFromClipboard(&buf)) {
    return;
  }
  PasteGameFromString( buf );
}

/* [AS] Try to detect whether the clipboard contains FEN or PGN data */
VOID PasteGameOrFENFromClipboard()
{
  char *buf;
  char *tmp;

  if (!PasteTextFromClipboard(&buf)) {
    return;
  }

  tmp = buf;
  while( *tmp == ' ' || *tmp == '\t' || *tmp == '\r' || *tmp == '\n' ) {
      tmp++;
  }

  if( *tmp == '[' ) {
      PasteGameFromString( buf );
  }
  else {
      PasteFENFromString( buf );
  }
}

int 
PasteTextFromClipboard(char **text)
{
  /* some (most?) of the error checking may be overkill, 
   * but hey, this is Windows 
   */
  HANDLE hClipMem;
  LPVOID lpClipMem;
  BOOL locked = FALSE;
  DWORD err;
  UINT lockCount;

  if (!OpenClipboard(hwndMain)) {
    DisplayError("Unable to open clipboard.", 0);
    return FALSE;
  }
  hClipMem = GetClipboardData(CF_TEXT);
  if (hClipMem == NULL) {
    CloseClipboard();
    DisplayError("No text in clipboard.", 0);
    return FALSE;
  }
  lpClipMem = GlobalLock(hClipMem);
  if (lpClipMem == NULL) {
    CloseClipboard();
    DisplayError("Unable to lock clipboard memory.", 0);
    return FALSE;
  }
  *text = (char *) malloc(GlobalSize(hClipMem)+1);
  if (!*text) {
    DisplayError("Unable to allocate memory for text string.", 0);
    CloseClipboard();
    return FALSE;
  }
  lstrcpy(*text, lpClipMem);
  if (appData.debugMode) {
    lockCount = GlobalFlags(hClipMem) & GMEM_LOCKCOUNT;
    fprintf(debugFP, "PasteTextFromClipboard(): lock count %d\n", lockCount);
  }
  SetLastError(NO_ERROR);
#if 1
  /*suggested by Wilkin Ng*/
  lockCount = GlobalFlags(hClipMem) & GMEM_LOCKCOUNT;
  if (lockCount) {
    locked = GlobalUnlock(hClipMem);
  }
#else
  locked = GlobalUnlock(hClipMem);
#endif
  err = GetLastError();
  if (appData.debugMode) {
    lockCount = GlobalFlags(hClipMem) & GMEM_LOCKCOUNT;
    fprintf(debugFP, "PasteTextFromClipboard(): lock count %d\n", lockCount);
  }
  if (!locked) {
    locked = !((err == NO_ERROR) || (err == ERROR_NOT_LOCKED));
    if (appData.debugMode) {
      fprintf(debugFP, 
	      "PasteTextFromClipboard(): err %d locked %d\n", err, locked);
    }
  }
  if (locked) 
    DisplayError("Unable to unlock clipboard memory.", 0);
  
  if (!CloseClipboard())
    DisplayError("Unable to close clipboard.", 0);
  
  return TRUE;
}

VOID
DeleteClipboardTempFiles()
{
  if (copyTemp) remove(copyTemp);
  if (pasteTemp) remove(pasteTemp);
}
