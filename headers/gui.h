#ifndef GUI_H
#define GUI_H

#include "usefulmacros.h"


#include <stdlib.h>
#include <stdio.h>

#include "raylib.h"
#include "raygui.h"

/* Show a ListBox with choices and return the choice that the user makes. */
const char *showListBox(char *choices[], int size);

#endif