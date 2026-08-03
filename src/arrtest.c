#include <stdio.h>
#include <stdlib.h>

int main(){
    const char **array[] = {
        {"1", "2"},
        {"3"},
        {"4", "5"}
    };

    for (int i = 0; i < sizeof(array) / sizeof(array[0]); i++){
        for (int j = 0; j < sizeof(array[i]) / sizeof(array[i][j]); j++){
            printf("Element: %s\n", array[i][j]);
        }
    }

    return 0;
}