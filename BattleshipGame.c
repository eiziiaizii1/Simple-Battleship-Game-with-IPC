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

int turnCount = 0;

struct ships {
    int size;
};

// Clear the grid memory using memset
void clear_grid(int grid[SIZE][SIZE]) {
    turnCount=0;
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
    turnCount++;
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

typedef struct {
    bool target_mode;     // Whether AI is in focused targeting mode
    int last_hit_x;       // X coordinate of the last successful hit
    int last_hit_y;       // Y coordinate of the last successful hit
    int hit_direction[2]; // Direction for targeting if orientation is determined (e.g., {0, 1} for horizontal)
    bool orientation_locked; // Whether AI has locked onto a horizontal or vertical orientation
} AIState;

AIState ai_state = {false, -1, -1, {0, 0}, false};

bool is_ship_sunk(int grid[SIZE][SIZE], int x, int y) {
    // Check if horizontally adjacent cells form a complete line of hits
    int left = x, right = x;
    while (left > 0 && grid[left - 1][y] == 9) left--;    // Expand to the left
    while (right < SIZE - 1 && grid[right + 1][y] == 9) right++;  // Expand to the right
    
    // Check if both ends of the horizontal line are blocked by misses or edges
    if ((left == 0 || grid[left - 1][y] == 8) && (right == SIZE - 1 || grid[right + 1][y] == 8)) {
        return true;  // Ship is sunk horizontally
    }
    
    // Check if vertically adjacent cells form a complete line of hits
    int up = y, down = y;
    while (up > 0 && grid[x][up - 1] == 9) up--;      // Expand upward
    while (down < SIZE - 1 && grid[x][down + 1] == 9) down++;  // Expand downward
    
    // Check if both ends of the vertical line are blocked by misses or edges
    if ((up == 0 || grid[x][up - 1] == 8) && (down == SIZE - 1 || grid[x][down + 1] == 8)) {
        return true;  // Ship is sunk vertically
    }

    return false;  // Ship is not yet fully hit and sunk
}

void aiAttack(int grid[SIZE][SIZE]) {
    int x, y;
    turnCount++;

    if (ai_state.target_mode) {
        bool found_target = false;

        if (ai_state.orientation_locked) {
            // Continue in the locked direction if orientation is determined
            x = ai_state.last_hit_x + ai_state.hit_direction[0];
            y = ai_state.last_hit_y + ai_state.hit_direction[1];

            // Check if within bounds and not previously attacked
            if (x >= 0 && x < SIZE && y >= 0 && y < SIZE &&
                grid[x][y] != 8 && grid[x][y] != 9) {
                found_target = true;
            } else {
                // Reverse direction if out of bounds or already hit
                ai_state.hit_direction[0] *= -1;
                ai_state.hit_direction[1] *= -1;
                x = ai_state.last_hit_x + ai_state.hit_direction[0];
                y = ai_state.last_hit_y + ai_state.hit_direction[1];
                found_target = (x >= 0 && x < SIZE && y >= 0 && y < SIZE &&
                                grid[x][y] != 8 && grid[x][y] != 9);
            }
        } else {
            // If orientation not locked, try all four directions around the last hit
            int directions[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
            for (int i = 0; i < 4; i++) {
                int new_x = ai_state.last_hit_x + directions[i][0];
                int new_y = ai_state.last_hit_y + directions[i][1];
                if (new_x >= 0 && new_x < SIZE && new_y >= 0 && new_y < SIZE &&
                    grid[new_x][new_y] != 8 && grid[new_x][new_y] != 9) {
                    x = new_x;
                    y = new_y;
                    found_target = true;
                    
                    // Lock orientation if a second adjacent hit is found
                    if (grid[x][y] == 1) {
                        ai_state.hit_direction[0] = directions[i][0];
                        ai_state.hit_direction[1] = directions[i][1];
                        ai_state.orientation_locked = true;
                    }
                    break;
                }
            }
        }

        if (!found_target) {
            ai_state.target_mode = false; // Reset if no adjacent targets are available
            ai_state.orientation_locked = false;
        }
    }

    // Random targeting mode if target_mode is false or no adjacent cells are available
    if (!ai_state.target_mode) {
        do {
            x = rand() % SIZE;
            y = rand() % SIZE;
        } while (grid[x][y] == 8 || grid[x][y] == 9);
    }

    if (grid[x][y] == 1) {  // Hit
        printf("AI hit at (%d, %d)!\n", x, y);
        grid[x][y] = 9;
        ai_state.last_hit_x = x;
        ai_state.last_hit_y = y;
        ai_state.target_mode = true;

        // Check if the ship is sunk
        if (is_ship_sunk(grid, x, y)) {
            printf("AI sunk a ship!\n");
            ai_state.target_mode = false;
            ai_state.orientation_locked = false;
        }
    } else {  // Miss
        printf("AI miss at (%d, %d)!\n", x, y);
        grid[x][y] = 8;
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
        srand(time(NULL) ^ getpid());

        int fd;
        while (true) {
            fd = open(fifo_name, O_RDONLY);
            int turn; // 1 --> parent / 0 --> child / -1 --> gameOver signal
            read(fd, &turn, sizeof(turn));
            close(fd);

            //usleep(1000);

            if (turn == -1) {
                printf("Child process exiting...\n");
                break;
            }
            
            printf("TURN COUNT: %d\n", turnCount);
            printf("Child's turn:\n");
            //attack((int (*)[SIZE])parent_grid);  // Cast to access as 2D array
            aiAttack((int (*)[SIZE])parent_grid);  // Cast to access as 2D array
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
        srand(time(NULL) ^ getpid()); 
        int fd;
        int turn = 1;
        while (true) {
            if (turn == 1) {
                
                //usleep(1000);
                printf("TURN COUNT: %d\n", turnCount);
                printf("Parent's turn:\n");
                //attack((int (*)[SIZE])child_grid);
                aiAttack((int (*)[SIZE])child_grid);
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
