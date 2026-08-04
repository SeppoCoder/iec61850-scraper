#ifndef USERPATH_HELPER_H
#define USERPATH_HELPER_H

#define JSMN_HEADER

#include "usefulmacros.h"
#include "xlsx_helpers.h"
#include "networking.h"

#include "jsmn.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct GeneralPath {
    char excelTitle[64];
    char paths[10][64];
    int excelColumn;
} GeneralPath;

/* Reads a whole file into the given buffer, the memory for the file buffer is manually allocated, which means that you must free it when done.  */
int readFileToString(const char *fileName, char *fileBuf[]);

/* Checks if some json string is equal to another string */
int jsoneq(const char *json, jsmntok_t *tok, const char *s);

/* Initializes the JSMN parser */
int initJsmn(jsmn_parser *parser, jsmntok_t **tokens, const char *fileContents, int fileSize, FILE *fptr);

/* Parse general paths from the given file contents. */
GeneralPath *parseGeneralPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int *maxStaticCol, int *count);

/* Parse and process user specific paths. */
void parseUserPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int tcpPort, int ipMinHost, lxw_worksheet *worksheet, int maxStaticCol, GeneralPath *path, int count, IPRange *ipRange);

#endif