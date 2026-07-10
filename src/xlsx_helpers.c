#include "xlsx_helpers.h"

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle, spreadsheetColumnMapping schema[], int numColumns) {
    for (int i = 0; i < numColumns; i++) {
        // Compare the target title with the schema
        if (strcmp(schema[i].title, targetTitle) == 0) {
            return i; 
        }
    }
    return -1; // didnt find the title
}

void initWorksheet(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, spreadsheetColumnMapping schema[], int numColumns){
    LOG(fptr, "Initializing worksheet...\n\n");
    
    worksheetError = worksheet_set_column(worksheet, 0, numColumns - 1, 20, NULL);
    if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't set column width, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }

    for(size_t i = 0; i < numColumns; i++){
        worksheetError = worksheet_write_string(worksheet, 0, i, schema[i].title, NULL);
        if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
    }
}