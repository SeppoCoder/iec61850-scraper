#include "userpath_helpers.h"
#include "iec61850_helpers.h"

int readFileToString(const char *fileName, char *fileBuf[]){
    FILE *fptr;
    fptr = fopen(fileName, "rb");
    if(!fptr){
        LOG(fptr,"failed to open file!\n");
        return -1;
    }

    fseek(fptr, 0, SEEK_END);   // jump to end of file
    long fileSize = ftell(fptr);    // calculate offset (size)
    rewind(fptr);  // Rewind back to the beginning of the file

    (*fileBuf) = (char *)malloc(fileSize + 1);
    if(!(*fileBuf)){
        LOG(fptr, "malloc failed\n");
        fclose(fptr);
        return -1;
    }

    size_t bytesRead = fread(*fileBuf, 1, fileSize, fptr);
   (*fileBuf)[bytesRead] = '\0';

    fclose(fptr);
    return bytesRead;
}


int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}


int initJsmn(jsmn_parser *parser, jsmntok_t **tokens, const char *fileContents, int fileSize, FILE *fptr){
    jsmn_init(parser);

    int tokenCount = jsmn_parse(parser, fileContents, fileSize, NULL, 0); // pass NULL as to get the size needed for the tokens
    if (tokenCount < 0){
        LOG(fptr, "Failed to get token size: %d\n", tokenCount);
        return -1;
    }

    *tokens = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokenCount); // initialize tokens with the needed size
    if (!tokens){
        LOG(fptr, "Failed to allocate memory for tokens!\n");
        return -1;
    }

    jsmn_init(parser);     // init again
    int readTokens = jsmn_parse(parser, fileContents, fileSize, *tokens, tokenCount);   // parse for real this time
    if (readTokens < 0){
        LOG(fptr, "Failed to parse JSON: %d\n", readTokens);
        return -1;
    }

    return readTokens;
}

void parseUserPaths(jsmn_parser *parser, jsmntok_t *tokens, const char *fileContents, int fileSize, int readTokens, FILE *fptr, int tcpPort, int ipMinHost, lxw_worksheet *worksheet, int maxStaticCol){
    if (readTokens < 1 || tokens[0].type != JSMN_OBJECT) {
        LOG(fptr, "Error while parsing user paths, Object expected!\n");
        return;
    }

    char titleBuf[64];
    char pathBuf[64];
    char ipBuf[24];              
    for (int i = 1; i < readTokens; i++){
        if (jsoneq(fileContents, &tokens[i], "Title") == 0){
            strncpy(titleBuf, fileContents + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start + 1);
            titleBuf[tokens[i + 1].end - tokens[i + 1].start] = '\0';
            LOG(fptr, "Title: %s\n", titleBuf);
            worksheet_write_string(worksheet, 0, maxStaticCol, titleBuf, NULL);
            worksheet_set_column(worksheet, maxStaticCol, maxStaticCol, 20, NULL);
            i++;
        }else if (jsoneq(fileContents, &tokens[i], "Path") == 0){   
            strncpy(pathBuf, fileContents + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start + 1);
            pathBuf[tokens[i + 1].end - tokens[i + 1].start] = '\0';
            LOG(fptr, "Path: %s\n", pathBuf);
            i++;
        }else if (jsoneq(fileContents, &tokens[i], "IpAddress") == 0){
            strncpy(ipBuf, fileContents + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start + 1);
            ipBuf[tokens[i + 1].end - tokens[i + 1].start] = '\0';
            LOG(fptr, "IpAddress: %s\n", ipBuf);
            processUserDevice(ipBuf, tcpPort, pathBuf, ipMinHost, worksheet, maxStaticCol, fptr);
            i++;
            maxStaticCol++;
        }
    }
}