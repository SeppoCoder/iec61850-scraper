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

    size_t bytesRead = fread(*fileBuf, 1, fileSize, fptr);  // read file contents into buffer
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

    jsmn_init(parser);     // Initialize again
    int readTokens = jsmn_parse(parser, fileContents, fileSize, *tokens, tokenCount);   // parse for real this time
    if (readTokens < 0){
        LOG(fptr, "Failed to parse JSON: %d\n", readTokens);
        free(*tokens);
        return -1;
    }

    return readTokens;
}

GeneralPath *parseGeneralPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int *maxStaticCol, int *count){
    if (readTokens < 1 || tokens[0].type != JSMN_OBJECT) {
        LOG(fptr, "Error while parsing user paths, Object expected!\n");
        return NULL;
    }
       
    GeneralPath *generalPaths;
    for (int i = 1; i < readTokens; i++){
        if (jsoneq(fileContents, &tokens[i], "GenericPaths") == 0){
            int tokenIndex = 1;
            *count = tokens[i + 2].size;
            generalPaths = (GeneralPath*) calloc(*count, sizeof(GeneralPath));  // allocate enough memory for all general paths
            if (!generalPaths) {
                LOG(fptr, "Couldn't allocate memory for general paths!\n");
                return NULL;
            }

            for (int j = 0; j < tokens[i + 2].size; j++){
                int baseTokenIndex = i + 2 + tokenIndex;
                strncpy(generalPaths[j].excelTitle, fileContents + tokens[baseTokenIndex].start, tokens[baseTokenIndex].end - tokens[baseTokenIndex].start + 1);
                generalPaths[j].excelTitle[tokens[baseTokenIndex].end - tokens[baseTokenIndex].start] = '\0';
                
                for (int k = 0; k < tokens[baseTokenIndex + 1].size; k++){
                    strncpy(generalPaths[j].paths[k], fileContents + tokens[baseTokenIndex + 2 + k].start, tokens[baseTokenIndex + 2 + k].end - tokens[baseTokenIndex + 2 + k].start + 1);
                    generalPaths[j].paths[k][tokens[baseTokenIndex + 2 + k].end - tokens[baseTokenIndex + 2 + k].start] = '\0';
                }
                generalPaths[j].excelColumn = *maxStaticCol;
                *maxStaticCol = *maxStaticCol + 1; 
                tokenIndex += tokens[i + 3 + tokenIndex].size + 2;
            } 
        }
    }
    return generalPaths;
} 

void parseUserPaths(jsmntok_t *tokens, const char *fileContents, int readTokens, FILE *fptr, int tcpPort, int ipMinHost, lxw_worksheet *worksheet, int maxCol, GeneralPath *path, int count, IPRange *ipRange){
    LOG(fptr, "Parsing User Specific Paths");
    if (readTokens < 1 || tokens[0].type != JSMN_OBJECT) {
        LOG(fptr, "Error while parsing user paths, Object expected!\n");
        return;
    }

    char titleBuf[64];
    char pathBuf[64];
    char ipBuf[24];
    int column = maxCol;    // initialize the column to be the first empty column
    bool readError;
    
    for (int i = 1; i < readTokens; i++){
        if (jsoneq(fileContents, &tokens[i], "Title") == 0){
            readError = false;
            snprintf(titleBuf, sizeof(titleBuf), "%.*s", (int)tokens[i + 1].end - (int)tokens[i + 1].start, fileContents + tokens[i + 1].start); // write contents of title attribute to buffer

            for (int pathIdx = 0; pathIdx < count; pathIdx++){
                // check if we already have this attribute title in the spreadsheet
                if (strcasecmp(titleBuf, path[pathIdx].excelTitle) == 0){
                    column = path[pathIdx].excelColumn; // update column to correspond to the column with the same title
                }
            }

            worksheet_write_string(worksheet, 0, column, titleBuf, NULL);   // write the title to the spreadsheet
            worksheet_set_column(worksheet, column, column, 30, NULL);  // set the column width
            i++;    // skip one token (to get next key)
        }else if (jsoneq(fileContents, &tokens[i], "Path") == 0){   
            int pathBytes = snprintf(pathBuf, sizeof(pathBuf), "%.*s", (int)tokens[i + 1].end - (int)tokens[i + 1].start, fileContents + tokens[i + 1].start);
            if (pathBytes < 0 || pathBytes >= sizeof(pathBuf)){
                LOG(fptr, "User path is either truncated, or couldn't be written to!\n");
                readError = true;
            }
            i++;
        }else if (jsoneq(fileContents, &tokens[i], "IpAddresses") == 0 && !readError){
            for (int idx = 0; idx < tokens[i + 1].size; idx++){
                int addressBytes = snprintf(ipBuf, sizeof(ipBuf), "%.*s", (int)tokens[i + 2 + idx].end - (int)tokens[i + 2 + idx].start, fileContents + tokens[i + 2 + idx].start);
                if (addressBytes < 0 || addressBytes >= sizeof(ipBuf)){
                    LOG(fptr, "User ip address (%s) is either truncated, or couldn't be written to!\n", ipBuf);
                    continue;
                }
                processUserDevice(ipBuf, tcpPort, pathBuf, ipMinHost, worksheet, column, fptr, ipRange); // process the user defined path
            }
            
            i++;
            maxCol++;   // increase maxCol to make sure the next user path will be written to the next empty spot in the spreadsheet
            column = maxCol;    // reset the column to that empty spot, in case we overwrote column with a previous column
        }
    }
}