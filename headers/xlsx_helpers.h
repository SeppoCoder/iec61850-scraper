#ifndef XLSX_HELPERS_H
#define XLSX_HELPERS_H

#include "xlsxwriter.h"

// struct for mapping spreadsheet title and data attribute
typedef struct {
    char *title;
    char *dataAttributeRef;
} spreadsheetColumnMapping;

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle, spreadsheetColumnMapping schema[], int numColumns);

void initWorksheet(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, spreadsheetColumnMapping schema[], int numColumns);

#endif