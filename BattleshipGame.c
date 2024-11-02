
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
    if(grid[x][y] == 0){
        grid[x][y] = 8;
    }
    else if(grid[x][y] == 1){
        grid[x][y] = 9;
    }
}

void playTurns(int parent_grid[SIZE][SIZE], int child_grid[SIZE][SIZE]) {
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

            // Game over signal
            if (turn == -1) {  
                printf("Child process exiting...\n");
                break;
            }

            printf("Child's turn:\n");
            attack(parent_grid);
            printf("Parent's Grid After Child's Attack\n");
            print_grid(parent_grid, "Parent");

            if (all_ships_sunk(parent_grid)) {
                printf("Child wins!\n");
                // Send game over signal to parent
                turn = -1;
                fd = open(fifo_name, O_WRONLY);
                write(fd, &turn, sizeof(turn));
                close(fd);
                break;
            }

            // Signal the parent's turn
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
                attack(child_grid);
                printf("Child's Grid After Parent's Attack\n");
                print_grid(child_grid, "Child");

                if (all_ships_sunk(child_grid)) {
                    printf("Parent wins!\n");
                    // Send game over signal to child
                    turn = -1;
                    fd = open(fifo_name, O_WRONLY);
                    write(fd, &turn, sizeof(turn));
                    close(fd);
                    break;
                }

                // Signal the child's turn
                turn = 0;
                fd = open(fifo_name, O_WRONLY);
                write(fd, &turn, sizeof(turn));
                close(fd);
            }

            // Wait for the child's turn or game over signal
            fd = open(fifo_name, O_RDONLY);
            read(fd, &turn, sizeof(turn));
            close(fd);

            // Game over signal received
            if (turn == -1) {  
                printf("Parent process exiting...\n");
                break;
            }
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

    // OPTION 1 VE OPTION 2 BIRBIRIYLE ALAKALI, OYUN BAŞLAR BAŞLAMAZ YENI GRID OLUŞTURACAK MI YOKSA HER PLAY NEW GAME DEDIĞINDE YENI GRID MI OLUŞTURACAK
    // HARITA OLUŞTURMADAN DISPLAY MAP YAPARSAK MATRIX SAÇMA SAPAN OLUYOR
    // DIREKT OYUNU OYNARKEN YENI HARITA OLUŞTURUNCA DA ILK BAŞTAKI OLUŞAN HARITANIN ÜZERİNE YAZIYOR

    // TODO-1  BUNA BI BAKMAK LAZIM GELIR

    // TODO-2 OYUN BITTIKTEN SONRA DISPLAY MAP YAPINCA CHILD PROCESS MATRIXIN İLK HALİNİ GÖSTERİYO, PARENT PROCESS İSE OYUN BİTTİĞİNDEKİ HALİNİ GÖSTERİYO  
    
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
                create_grid(parent_grid);
                create_grid(child_grid);

                place_ships(parent_grid, ship_list);
                place_ships(child_grid, ship_list);

                playTurns(parent_grid, child_grid);

 			//create processes here 
			/*
			pid_t pid = fork();
    struct ships ship_list[SHIP_COUNT] = { {4}, {3}, {3}, {2} };
    place_ships(parent_grid, ship_list);
    place_ships(child_grid, ship_list);

    print_grid(parent_grid, "Parent");
    print_grid(child_grid, "Child");

    // Start the game
    playTurns(parent_grid, child_grid);

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
