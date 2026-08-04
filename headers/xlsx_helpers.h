#ifndef XLSX_HELPERS_H
#define XLSX_HELPERS_H

#pragma once

#include "usefulmacros.h"

#include "xlsxwriter.h"

typedef struct GeneralPath GeneralPath; // define GeneralPath as to be able to use it in the function without including the header (created circular dependency)

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle, const char *predefinedColumns[], int numColumns);

/* Initializes worksheet with the use of predefined spreadsheet titles. */
void initWorksheet(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, const char *predefinedColumns[], int numColumns);

/* Writes Titles to the worksheet from the given GeneralPath. */
void writeWorksheetTitles(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, GeneralPath *path, int count, int numberOfColumns);

#endif