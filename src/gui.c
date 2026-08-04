#define RAYGUI_IMPLEMENTATION
#include "gui.h"

const char *showListBox(char *choices[], int size){
    InitWindow(400, 300, "Nätverk");
    SetTargetFPS(60);

    // Variables to track our list's state
    int activeItem = -1; // Which item is clicked/selected
    int focusItem = -1;  // which item is in focus
    int scrollIndex = 0; // Tracks scrolling if the list gets too long

    while (!GuiButton((Rectangle){ 150, 220, 100, 40 }, "Välj")) {
        
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*2); 

        DrawText("Välj ett nätverk!", 50, 10, TEXT_SIZE * 2, BLACK);

        // Draw the ListBox
        GuiListViewEx((Rectangle){ 50, 50, 300, 150 }, choices, size, &scrollIndex, &activeItem, &focusItem);
    
        EndDrawing();
    }

    // Clean up and close
    CloseWindow();
    if(activeItem != -1){
        return choices[activeItem];
    }
    return NULL;
}