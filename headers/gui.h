#ifndef GUI_H
#define GUI_H

#include <stdlib.h>
#include <stdio.h>

#include "raylib.h"
#include "raygui.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))

/* Show a ListBox with choices and return the choice that the user makes. */
const char *showListBox(char *choices[], int size);

#endif