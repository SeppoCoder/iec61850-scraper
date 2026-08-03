#ifndef USERPATH_HELPER_H
#define USERPATH_HELPER_H

#define JSMN_HEADER

#pragma once

#include "usefulmacros.h"
#include "xlsx_helpers.h"

#include "jsmn.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char excelTitle[64];
    char pathToDa[64];
    char ipAddr[24];
} UserPath;

typedef struct GeneralPath {
    char excelTitle[64];
    char paths[10][64];
    int excelColumn;
} GeneralPath;

int readFileToString(const char *fileName, char *fileBuf[]);

int jsoneq(const char *json, jsmntok_t *tok, const char *s);

int initJsmn(jsmn_parser *parser, jsmntok_t **tokens, const char *fileContents, int fileSize, FILE *fptr);

GeneralPath *parseGeneralPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int *maxStaticCol, int *count);

void parseUserPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int tcpPort, int ipMinHost, lxw_worksheet *worksheet, int maxStaticCol, GeneralPath *path, int count);

#endif