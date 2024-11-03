#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/mman.h>
#include <string.h>

#define SIZE 8
#define SHIP_COUNT 4

struct ships {
    int size;
};

// Clear the grid memory using memset
void clear_grid(int grid[SIZE][SIZE]) {
    memset(grid, 0, SIZE * SIZE * sizeof(int));
}

bool can_place_ship(int grid[SIZE][SIZE], int x, int y, int size, bool horizontal) {
    if (horizontal) {
        // Check if the ship would exceed the grid horizontally
        if (y + size > SIZE) return false;

        for (int i = 0; i < size; i++) {
            // Check main cells for placement
            if (grid[x][y + i] == 1) return false;

            // Check cells above and below the ship's placement (within bounds)
            if (x > 0 && grid[x - 1][y + i] == 1) return false;
            if (x < SIZE - 1 && grid[x + 1][y + i] == 1) return false;
        }

        // Check left side edge (within bounds)
        if (y > 0) {
            if (grid[x][y - 1] == 1) return false;
            if (x > 0 && grid[x - 1][y - 1] == 1) return false;
            if (x < SIZE - 1 && grid[x + 1][y - 1] == 1) return false;
        }

        // Check right side edge (within bounds)
        if (y + size < SIZE) {
            if (grid[x][y + size] == 1) return false;
            if (x > 0 && grid[x - 1][y + size] == 1) return false;
            if (x < SIZE - 1 && grid[x + 1][y + size] == 1) return false;
        }
    } else {
        // Check if the ship would exceed the grid vertically
        if (x + size > SIZE) return false;

        for (int i = 0; i < size; i++) {
            // Check main cells for placement
            if (grid[x + i][y] == 1) return false;

            // Check cells to the left and right of the ship's placement (within bounds)
            if (y > 0 && grid[x + i][y - 1] == 1) return false;
            if (y < SIZE - 1 && grid[x + i][y + 1] == 1) return false;
        }

        // Check top edge (within bounds)
        if (x > 0) {
            if (grid[x - 1][y] == 1) return false;
            if (y > 0 && grid[x - 1][y - 1] == 1) return false;
            if (y < SIZE - 1 && grid[x - 1][y + 1] == 1) return false;
        }

        // Check bottom edge (within bounds)
        if (x + size < SIZE) {
            if (grid[x + size][y] == 1) return false;
            if (y > 0 && grid[x + size][y - 1] == 1) return false;
            if (y < SIZE - 1 && grid[x + size][y + 1] == 1) return false;
        }
    }
    return true;
}

// Place ship within safe bounds
void place_ship(int grid[SIZE][SIZE], int x, int y, int size, bool horizontal) {
    if (horizontal) {
        for (int i = 0; i < size; i++, y++) {
            grid[x][y] = 1;
        }
    } 
    else {
        for (int i = 0; i < size; i++, x++) {
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

// Currently Attacks randomly, will be changed according to ai behavior
void attack(int grid[SIZE][SIZE]) {
    int x, y;
    
    // Avoids hitting the previously hit cells
    do {
        x = rand() % SIZE;
        y = rand() % SIZE;
    } while (grid[x][y] == 8 || grid[x][y] == 9);

    if (grid[x][y] == 1) {
        printf("Hit at (%d, %d)!\n", x, y);
    } else {
        printf("Miss at (%d, %d)!\n", x, y);
    }
    // 8 = miss
    // 9 = hit
    if(grid[x][y] == 0){
        grid[x][y] = 8;
    }
    else if(grid[x][y] == 1){
        grid[x][y] = 9;
    }
}

void playTurns(int *parent_grid, int *child_grid) {
    const char *fifo_name = "/tmp/battleship_fifo";
    mkfifo(fifo_name, 0666);

    pid_t pid = fork();

    if (pid < 0) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }
    // Child process
    else if (pid == 0) {  
        int fd;
        while (true) {
            fd = open(fifo_name, O_RDONLY);
            int turn; // 1 --> parent / 0 --> child / -1 --> gameOver signal
            read(fd, &turn, sizeof(turn));
            close(fd);

            if (turn == -1) {
                printf("Child process exiting...\n");
                break;
            }

            printf("Child's turn:\n");
            attack((int (*)[SIZE])parent_grid);  // Cast to access as 2D array
            printf("Parent's Grid After Child's Attack\n");
            print_grid((int (*)[SIZE])parent_grid, "Parent");

            if (all_ships_sunk((int (*)[SIZE])parent_grid)) {
                printf("Child wins!\n");
                turn = -1;
                fd = open(fifo_name, O_WRONLY);
                write(fd, &turn, sizeof(turn));
                close(fd);
                break;
            }

            turn = 1;
            fd = open(fifo_name, O_WRONLY);
            write(fd, &turn, sizeof(turn));
            close(fd);
        }
        exit(0);
    } 
    // Parent process
    else {  
        int fd;
        int turn = 1;
        while (true) {
            if (turn == 1) {
                printf("Parent's turn:\n");
                attack((int (*)[SIZE])child_grid);
                printf("Child's Grid After Parent's Attack\n");
                print_grid((int (*)[SIZE])child_grid, "Child");

                if (all_ships_sunk((int (*)[SIZE])child_grid)) {
                    printf("Parent wins!\n");
                    turn = -1;
                    fd = open(fifo_name, O_WRONLY);
                    write(fd, &turn, sizeof(turn));
                    close(fd);
                    break;
                }

                turn = 0;
                fd = open(fifo_name, O_WRONLY);
                write(fd, &turn, sizeof(turn));
                close(fd);
            }

            fd = open(fifo_name, O_RDONLY);
            read(fd, &turn, sizeof(turn));
            close(fd);

            if (turn == -1) {  
                printf("Parent process exiting...\n");
                break;
            }
        }

        waitpid(pid, NULL, 0);
        unlink(fifo_name);
    }
}

int main() {
    srand(time(NULL));

    // Allocate grids in shared memory once
    int (*parent_grid)[SIZE] = mmap(NULL, SIZE * SIZE * sizeof(int),
                                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int (*child_grid)[SIZE] = mmap(NULL, SIZE * SIZE * sizeof(int),
                                   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // Check if mmap was successful
    if (parent_grid == MAP_FAILED || child_grid == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Initialize grids with memset for reliability
    clear_grid(parent_grid);
    clear_grid(child_grid);

    bool shipsPlaced = false;
    bool alreadyBattled = false;

    struct ships ship_list[SHIP_COUNT] = { {4}, {3}, {3}, {2} };

    printf("Welcome to The Battleship Game\n");
    int option;
    do {
        printf("Enter 1 to place/replace the ships\n");
        printf("Enter 2 to start the battle\n");
        printf("Enter 3 to reset the map\n");
        printf("Enter 4 to display map\n");
        printf("Press any key to finish the game\n");

        if (scanf("%d", &option) != 1) {
            break;
        }

        if (option == 1) {
            clear_grid(parent_grid);
            clear_grid(child_grid);

            printf("Placing ships...\n");
            place_ships(parent_grid, ship_list);
            place_ships(child_grid, ship_list);
            printf("Ships placed successfully.\n");
            shipsPlaced = true;
            alreadyBattled = false;

        }
        else if (option == 2) {
            if (!shipsPlaced) {
                printf("\nPLACE THE SHIPS FIRST!!!\n");
            }
            else if (alreadyBattled) {
                printf("\n YOU MUST RESET THE MAP AND REPLACE THE SHIPS!!!\n");
            }
            else {
                playTurns((int *)parent_grid, (int *)child_grid);
                alreadyBattled = true;
            }
        }
        else if (option == 3) {
            clear_grid(parent_grid);
            clear_grid(child_grid);

            alreadyBattled = false;
            shipsPlaced = false;
        } 
        else if (option == 4) {
            printf("Parent Process: Displaying Parent's Grid\n");
            print_grid(parent_grid, "Parent");
            printf("Child Process: Displaying Child's Grid\n");
            print_grid(child_grid, "Child");
        }

        printf("\n----------------------------------------------------------\n");
    } while (option > 0 && option < 5);

    // Clean up memory at the end
    munmap(parent_grid, SIZE * SIZE * sizeof(int));
    munmap(child_grid, SIZE * SIZE * sizeof(int));

    return 0;
}
