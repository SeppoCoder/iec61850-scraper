#ifndef XLSX_HELPERS_H
#define XLSX_HELPERS_H

#pragma once

#include "usefulmacros.h"

#include "xlsxwriter.h"

typedef struct GeneralPath GeneralPath;

// struct for mapping spreadsheet title and data attribute
typedef struct {
    char *title;
    char *dataAttributeRef;
} spreadsheetColumnMapping;

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle, const char *predefinedColumns[], int numColumns);

void initWorksheet(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, const char *predefinedColumns[], int numColumns);

void writeWorksheetTitles(lxw_worksheet *worksheet, lxw_error worksheetError, FILE *fptr, GeneralPath *path, int count, int numberOfColumns);

#endif