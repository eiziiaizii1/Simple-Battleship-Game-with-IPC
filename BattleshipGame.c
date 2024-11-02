#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


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
        }
    } else {
        if(x + size > SIZE) return false;
        for (int i = 0; i < size; i++) {
            if(grid[x + i][y] == 1) return false;
        }
    }
    return true;
}

void place_ship(int grid[SIZE][SIZE], int x, int y, int size, bool horizontal){
    if(horizontal) {
        for (int i = 0; i < size; i++) {
            grid[x][y + i] = 1;
        }
    }
    else {
        for (int i = 0; i < size; i++) {
            grid[x + i][y] = 1;
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

// Currently Attacks randomly, will be changed according to ai behavior
void attack(int grid[SIZE][SIZE]) {
    int x, y;
    
    // Avoids hitting the previously hit cells
    do {
        x = rand() % SIZE;
        y = rand() % SIZE;
    } while (grid[x][y] == 2 || grid[x][y] == 2);

    if (grid[x][y] == 1) {
        printf("Hit at (%d, %d)!\n", x, y);
    } else {
        printf("Miss at (%d, %d)!\n", x, y);
    }
    // 8 = miss
    // 9 = hit
    grid[x][y] = (grid[x][y] == 0 ? 8 : 9); 
}

void playTurns(int parent_grid[SIZE][SIZE], int child_grid[SIZE][SIZE]) {
    //creation of named pipeline
    const char *fifo_name = "/tmp/battleship_fifo";
    mkfifo(fifo_name, 0666); // read and write access for all users

    pid_t pid = fork();

    if (pid < 0) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }
    // Child process
    else if (pid == 0) {
        int fd;
        while (!all_ships_sunk(parent_grid)) {
            // Wait for the parent's turn to complete
            fd = open(fifo_name, O_RDONLY); // file descriptor
            int turn = 1; // 1 -> parent's turn, 0 --> child's turn  
            read(fd, &turn, sizeof(turn));
            close(fd);

            printf("Child's turn:\n");
            attack(parent_grid);
            printf("Parent's Grid After Child's Attack\n");
            print_grid(parent_grid, "Parent");

            if (all_ships_sunk(parent_grid)) {
                printf("Child wins!\n");
                break;
            }

            // Signal the parent's turn
            turn = 1;
            fd = open(fifo_name, O_WRONLY);
            write(fd, &turn, sizeof(turn));
            close(fd);
        }
    } 
    // Parent process
    else {  
        int fd;
        int turn = 1;
        while (!all_ships_sunk(child_grid)) {
            if (turn == 1) {
                printf("Parent's turn:\n");
                attack(child_grid);
                printf("Child's Grid After Parent's Attack\n");
                print_grid(child_grid, "Child");

                if (all_ships_sunk(child_grid)) {
                    printf("Parent wins!\n");
                    break;
                }

                // Signal the child's turn
                turn = 0;
                fd = open(fifo_name, O_WRONLY);
                write(fd, &turn, sizeof(turn));
                close(fd);
            }

            // Wait for the child's turn to complete
            fd = open(fifo_name, O_RDONLY);
            read(fd, &turn, sizeof(turn));
            close(fd);
        }

        // Clean up
        wait(NULL);
        unlink(fifo_name);
    }
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

    print_grid(parent_grid, "Parent");
    print_grid(child_grid, "Child");

    // Start the game
    playTurns(parent_grid, child_grid);

    return 0;
}
