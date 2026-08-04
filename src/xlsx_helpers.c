#include "xlsx_helpers.h"
#include "userpath_helpers.h"

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle, const char *predefinedColumns[], int numColumns) {
    for (int i = 0; i < numColumns; i++) {
        // Compare the target title with the schema
        if (strcmp(predefinedColumns[i], targetTitle) == 0) {
            return i; 
        }
    }
    return -1; // didnt find the title
}

void initWorksheet(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, const char *predefinedColumns[], int numColumns){
    LOG(fptr, "Initializing worksheet...\n\n");
    
    worksheetError = worksheet_set_column(worksheet, 0, numColumns - 1, 20, NULL);  // set column width
    if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't set column width, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }

    for(size_t i = 0; i < numColumns; i++){
        worksheetError = worksheet_write_string(worksheet, 0, i, predefinedColumns[i], NULL);   // write each title to row 0
        if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
    }
}

void writeWorksheetTitles(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, GeneralPath *path, int count, int numberOfColumns){
    LOG(fptr, "writing worksheet titles...\n\n");

    for (size_t i = 0; i < count; i++){
       int offset = numberOfColumns + i;    // which column to write in
        worksheetError = worksheet_set_column(worksheet, offset, offset, 30, NULL); // set column width
        if(worksheetError != LXW_NO_ERROR){
                LOG(fptr, "Couldn't set column width, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }

        worksheetError = worksheet_write_string(worksheet, 0, offset, path[i].excelTitle, NULL);    // write each title to the first row in the spreadsheet
        if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
    }
}