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

typedef struct {
    bool target_mode;     // Whether AI is in focused targeting mode
    int last_hit_x;       // X coordinate of the last successful hit
    int last_hit_y;       // Y coordinate of the last successful hit
    int hit_direction[2]; // Direction for targeting if orientation is determined
    bool orientation_locked; // Whether AI has locked onto horizontal or vertical
    int first_hit_x;      // X coordinate of the first hit on current ship
    int first_hit_y;      // Y coordinate of the first hit on current ship
    bool direction_reversed; // Whether we've already tried reversing direction
} AIState;

AIState ai_state = {false, -1, -1, {0, 0}, false, -1, -1, false};
int turnCount = 0;

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

// Helper function to mark adjacent cells as misses when a ship is sunk
void mark_adjacent_cells(int grid[SIZE][SIZE], int start_x, int start_y, int end_x, int end_y) {
    // Determine the area to mark
    int min_x = (start_x < end_x ? start_x : end_x) - 1;
    int max_x = (start_x > end_x ? start_x : end_x) + 1;
    int min_y = (start_y < end_y ? start_y : end_y) - 1;
    int max_y = (start_y > end_y ? start_y : end_y) + 1;

    // Mark all cells in the area
    for (int i = min_x; i <= max_x; i++) {
        for (int j = min_y; j <= max_y; j++) {
            // Check if within grid bounds and not a hit
            if (i >= 0 && i < SIZE && j >= 0 && j < SIZE && grid[i][j] != 9) {
                grid[i][j] = 8; // Mark as miss
            }
        }
    }
}

bool is_ship_sunk(int grid[SIZE][SIZE], int x, int y, bool mark_adjacent) {
    // First, verify we're actually checking a hit position
    if (grid[x][y] != 9) {
        return false;
    }

    // Store ship coordinates for adjacent cell marking
    int start_x = x, end_x = x;
    int start_y = y, end_y = y;

    // Check both horizontal and vertical directions from the hit position
    int horizontal_length = 1;
    int vertical_length = 1;
    bool horizontal_complete = true;
    bool vertical_complete = true;

    // Check horizontal direction
    // Left
    int left = y - 1;
    while (left >= 0 && (grid[x][left] == 9 || grid[x][left] == 1)) {
        if (grid[x][left] == 1) {
            horizontal_complete = false;
        }
        horizontal_length++;
        start_y = left;
        left--;
    }
    
    // Right
    int right = y + 1;
    while (right < SIZE && (grid[x][right] == 9 || grid[x][right] == 1)) {
        if (grid[x][right] == 1) {
            horizontal_complete = false;
        }
        horizontal_length++;
        end_y = right;
        right++;
    }

    // Check vertical direction
    // Up
    int up = x - 1;
    while (up >= 0 && (grid[up][y] == 9 || grid[up][y] == 1)) {
        if (grid[up][y] == 1) {
            vertical_complete = false;
        }
        vertical_length++;
        start_x = up;
        up--;
    }
    
    // Down
    int down = x + 1;
    while (down < SIZE && (grid[down][y] == 9 || grid[down][y] == 1)) {
        if (grid[down][y] == 1) {
            vertical_complete = false;
        }
        vertical_length++;
        end_x = down;
        down++;
    }

    // Validate ship configuration
    if (horizontal_length > 1 && vertical_length > 1) {
        return false; // Invalid L-shaped configuration
    }

    bool is_sunk = false;
    // Check if we found a complete
    // Horizontal ship
    if (horizontal_length > 1) {
        bool properly_bounded = 
            (left < 0 || grid[x][left] == 8 || grid[x][left] == 0) &&
            (right >= SIZE || grid[x][right] == 8 || grid[x][right] == 0);
            
        is_sunk = properly_bounded && horizontal_complete;
        if (is_sunk && mark_adjacent) {
            mark_adjacent_cells(grid, x, start_y, x, end_y);
        }
    }

    // Vertical sihp
    if (vertical_length > 1) {
        bool properly_bounded = 
            (up < 0 || grid[up][y] == 8 || grid[up][y] == 0) &&
            (down >= SIZE || grid[down][y] == 8 || grid[down][y] == 0);
            
        is_sunk = properly_bounded && vertical_complete;
        if (is_sunk && mark_adjacent) {
            mark_adjacent_cells(grid, start_x, y, end_x, y);
        }
    }

    return is_sunk;
}

void aiAttack(int grid[SIZE][SIZE]) {
    int x, y;
    turnCount++;

    if (ai_state.target_mode) {
        bool found_target = false;

        if (ai_state.orientation_locked) {
            // Try current direction first
            x = ai_state.last_hit_x + ai_state.hit_direction[0];
            y = ai_state.last_hit_y + ai_state.hit_direction[1];

            // Check if we need to reverse direction
            if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || 
                grid[x][y] == 8 || grid[x][y] == 9) {
                
                if (!ai_state.direction_reversed) {
                    // Reverse direction and start from first hit
                    ai_state.hit_direction[0] *= -1;
                    ai_state.hit_direction[1] *= -1;
                    ai_state.direction_reversed = true;
                    
                    // Start from the first hit position
                    x = ai_state.first_hit_x + ai_state.hit_direction[0];
                    y = ai_state.first_hit_y + ai_state.hit_direction[1];
                    
                    if (x >= 0 && x < SIZE && y >= 0 && y < SIZE && 
                        grid[x][y] != 8 && grid[x][y] != 9) {
                        found_target = true;
                    }
                }
                
                if (!found_target) {
                    ai_state.target_mode = false;
                    ai_state.orientation_locked = false;
                    ai_state.direction_reversed = false;
                }
            } else {
                found_target = true;
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
                    break;
                }
            }
        }

        if (!found_target) {
            // Reset to random targeting if no valid targets found
            do {
                x = rand() % SIZE;
                y = rand() % SIZE;
            } while (grid[x][y] == 8 || grid[x][y] == 9);
            
            ai_state.target_mode = false;
            ai_state.orientation_locked = false;
            ai_state.direction_reversed = false;
        }
    } else {
        // Random targeting mode
        do {
            x = rand() % SIZE;
            y = rand() % SIZE;
        } while (grid[x][y] == 8 || grid[x][y] == 9);
    }

    // Process the attack
    if (grid[x][y] == 1) {
        printf("AI hit at (%d, %d)!\n", x, y);
        grid[x][y] = 9;
        
        if (!ai_state.target_mode) {
            // First hit on a new ship
            ai_state.first_hit_x = x;
            ai_state.first_hit_y = y;
            ai_state.direction_reversed = false;
        } else if (!ai_state.orientation_locked) {
            // Second hit - LETS GOOO LOCK IN TIMEE
            ai_state.hit_direction[0] = x - ai_state.last_hit_x;
            ai_state.hit_direction[1] = y - ai_state.last_hit_y;
            ai_state.orientation_locked = true;
        }
        
        ai_state.last_hit_x = x;
        ai_state.last_hit_y = y;
        ai_state.target_mode = true;

        // Check if the ship is sunk, passing true to mark adjacent cells
        if (is_ship_sunk(grid, x, y, true)) {
            printf("AI sunk a ship!\n");
            ai_state.target_mode = false;
            ai_state.orientation_locked = false;
            ai_state.direction_reversed = false;
        }
    } else {  // Miss
        printf("AI miss at (%d, %d)!\n", x, y);
        grid[x][y] = 8;
    }
}


// AI that doesn't know adjaceny rule 
/*
void aiAttackNOOBIE(int grid[SIZE][SIZE]) {
    int x, y;
    turnCount++;

    if (ai_state.target_mode) {
        bool found_target = false;

        if (ai_state.orientation_locked) {
            // Try current direction first
            x = ai_state.last_hit_x + ai_state.hit_direction[0];
            y = ai_state.last_hit_y + ai_state.hit_direction[1];

            // Check if we need to reverse direction
            if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || 
                grid[x][y] == 8 || grid[x][y] == 9) {
                
                if (!ai_state.direction_reversed) {
                    // Reverse direction and start from first hit
                    ai_state.hit_direction[0] *= -1;
                    ai_state.hit_direction[1] *= -1;
                    ai_state.direction_reversed = true;
                    
                    // Start from the first hit position
                    x = ai_state.first_hit_x + ai_state.hit_direction[0];
                    y = ai_state.first_hit_y + ai_state.hit_direction[1];
                    
                    if (x >= 0 && x < SIZE && y >= 0 && y < SIZE && 
                        grid[x][y] != 8 && grid[x][y] != 9) {
                        found_target = true;
                    }
                }
                
                if (!found_target) {
                    // If we've already tried both directions, reset targeting
                    ai_state.target_mode = false;
                    ai_state.orientation_locked = false;
                    ai_state.direction_reversed = false;
                }
            } else {
                found_target = true;
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
                    break;
                }
            }
        }

        if (!found_target) {
            // Reset to random targeting if no valid targets found
            do {
                x = rand() % SIZE;
                y = rand() % SIZE;
            } while (grid[x][y] == 8 || grid[x][y] == 9);
            
            ai_state.target_mode = false;
            ai_state.orientation_locked = false;
            ai_state.direction_reversed = false;
        }
    } else {
        // Random targeting mode
        do {
            x = rand() % SIZE;
            y = rand() % SIZE;
        } while (grid[x][y] == 8 || grid[x][y] == 9);
    }

    // Process the attack
    if (grid[x][y] == 1) {  // Hit
        printf("AI hit at (%d, %d)!\n", x, y);
        grid[x][y] = 9;
        
        if (!ai_state.target_mode) {
            // First hit on a new ship
            ai_state.first_hit_x = x;
            ai_state.first_hit_y = y;
            ai_state.direction_reversed = false;
        } else if (!ai_state.orientation_locked) {
            // Second hit - lock orientation
            ai_state.hit_direction[0] = x - ai_state.last_hit_x;
            ai_state.hit_direction[1] = y - ai_state.last_hit_y;
            ai_state.orientation_locked = true;
        }
        
        ai_state.last_hit_x = x;
        ai_state.last_hit_y = y;
        ai_state.target_mode = true;

        // Check if the ship is sunk
        if (is_ship_sunk(grid, x, y)) {
            printf("AI sunk a ship!\n");
            ai_state.target_mode = false;
            ai_state.orientation_locked = false;
            ai_state.direction_reversed = false;
        }
    } else {  // Miss
        printf("AI miss at (%d, %d)!\n", x, y);
        grid[x][y] = 8;
    }
}
bool is_ship_sunk(int grid[SIZE][SIZE], int x, int y) {
    if (grid[x][y] != 9) {
        return false;
    }

    int horizontal_length = 1;
    int vertical_length = 1;
    bool horizontal_complete = true;
    bool vertical_complete = true;

    int left = y - 1;
    while (left >= 0 && (grid[x][left] == 9 || grid[x][left] == 1)) {
        if (grid[x][left] == 1) {
            horizontal_complete = false;
        }
        horizontal_length++;
        left--;
    }
    
    int right = y + 1;
    while (right < SIZE && (grid[x][right] == 9 || grid[x][right] == 1)) {
        if (grid[x][right] == 1) {
            horizontal_complete = false;
        }
        horizontal_length++;
        right++;
    }

    int up = x - 1;
    while (up >= 0 && (grid[up][y] == 9 || grid[up][y] == 1)) {
        if (grid[up][y] == 1) {
            vertical_complete = false;
        }
        vertical_length++;
        up--;
    }
    
    int down = x + 1;
    while (down < SIZE && (grid[down][y] == 9 || grid[down][y] == 1)) {
        if (grid[down][y] == 1) {
            vertical_complete = false;
        }
        vertical_length++;
        down++;
    }

    if (horizontal_length > 1 && vertical_length > 1) {
        return false;
    }

    if (horizontal_length > 1) {
        bool properly_bounded = 
            (left < 0 || grid[x][left] == 8 || grid[x][left] == 0) &&
            (right >= SIZE || grid[x][right] == 8 || grid[x][right] == 0);
            
        return properly_bounded && horizontal_complete;
    }

    if (vertical_length > 1) {
        bool properly_bounded = 
            (up < 0 || grid[up][y] == 8 || grid[up][y] == 0) &&
            (down >= SIZE || grid[down][y] == 8 || grid[down][y] == 0);
            
        return properly_bounded && vertical_complete;
    }

    return false;
}
*/

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
