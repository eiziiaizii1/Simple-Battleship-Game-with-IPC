
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define SIZE 8
#define SHIP_COUNT 4

struct ships {
    int size;
};

void create_grid(int grid[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            grid[i][j] = 0;
        }
    }
}

bool can_place_ship(int grid[SIZE][SIZE], int x, int y, int size, bool horizontal) {
    if (horizontal) {
        if(y + size > SIZE) return false;
        for (int i = 0; i < size; i++) {
            if(grid[x][y + i] == 1) return false;
            if(x > 0 && grid[x - 1][y + i] == 1) return false;
            if(x < SIZE - 1 && grid[x + 1][y + i] == 1) return false;
        }
        if (y > 0 && grid[x][y - 1] == 1 || grid[x - 1][y - 1] == 1 || grid[x + 1][y - 1] == 1) return false;
        if (y + size < SIZE && grid[x][y + size] == 1 || grid[x - 1][y + size] == 1 || grid[x + 1][y + size] == 1) return false;
    } else {
        if(x + size > SIZE) return false;
        for (int i = 0; i < size; i++) {
            if(grid[x + i][y] == 1) return false;
            if(y > 0 && grid[x + i][y - 1] == 1) return false;
            if(y < SIZE - 1 && grid[x + i][y + 1] == 1) return false;
        }
        if (x > 0 && grid[x - 1][y] == 1 || grid[x - 1][y + 1] == 1|| grid[x - 1][y - 1] == 1) return false;
        if (x + size < SIZE && grid[x + size][y] == 1 || grid[x + size][y + 1] == 1 || grid[x + size][y - 1] == 1) return false;
    }
    return true;
}

void place_ship(int grid[SIZE][SIZE], int x, int y, int size, bool horizontal){
    if(horizontal) {
        for (int i = 0; i < size; i++, y++){
            grid[x][y] = 1;
        }
    }
    else {
        for (int i = 0; i < size; i++, x++){
            grid[x][y] = 1;
        }
    }
}

void place_ships(int grid[SIZE][SIZE], struct ships ship_list[SHIP_COUNT]) {
    for (int i = 0; i < SHIP_COUNT; i++) {
        bool placed = false;
        while (!placed) {
            int x = rand() % SIZE;
            int y = rand() % SIZE;
            bool horizontal = rand() % 2;
            if (can_place_ship(grid, x, y, ship_list[i].size, horizontal)) {
                place_ship(grid, x, y, ship_list[i].size, horizontal);
                placed = true;
            }
        }
    }
}

bool all_ships_sunk(int grid[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (grid[i][j] == 1) {
                return false;
            }
        }
    }
    return true;
};

void print_grid(int grid[SIZE][SIZE], const char *name) {
    printf("%s's grid:\n", name);
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            printf("%d ", grid[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
	srand(time(NULL));

    	int parent_grid[SIZE][SIZE];
    	int child_grid[SIZE][SIZE];

    	create_grid(parent_grid);
    	create_grid(child_grid);

	struct ships ship_list[SHIP_COUNT] = { {4}, {3}, {3}, {2} };

    	place_ships(parent_grid, ship_list);
    	place_ships(child_grid, ship_list);
	printf("Welcome to The Battleship Game\n");
	int option;
	do{
		printf("Enter 1 to start a new game\n");
      		printf("Enter 2 to display map\n");
      		printf("Enter 3 to change the location of the ships\n");
      		printf("Press any key to finish the game\n");

      		if (scanf("%d", &option) != 1){
         		break;
      		}

      		if(option == 1){
 			//create processes here 
			/*
			pid_t pid = fork();

		        if (pid < 0) {
                		perror("Failed to fork");
               			exit(EXIT_FAILURE);
        		}else if (pid == 0) {
                		printf("Child Process: Displaying Child's Grid\n");
                		print_grid(child_grid, "Child");
                		exit(0);
        		}else {
                		printf("Parent Process: Displaying Parent's Grid\n");
                		print_grid(parent_grid, "Parent");
			}*/
		}else if(option == 2){
			printf("Child Process: Displaying Child's Grid\n");
                	print_grid(child_grid, "Child");
			printf("Parent Process: Displaying Parent's Grid\n");
                	print_grid(parent_grid, "Parent");
    		}else if(option == 3){
			create_grid(parent_grid);
                	create_grid(child_grid);
                        place_ships(parent_grid, ship_list);
                	place_ships(child_grid, ship_list);
      		}

		printf("\n\n");
	}while(option >0 && option <4);
	

	return 0;
}
