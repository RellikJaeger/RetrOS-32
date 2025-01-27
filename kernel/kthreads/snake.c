/**
 * @file snake.c
 * @author Joe Bayer (joexbayer)
 * @brief Textmode only snake game
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#define SNAKE_LENGTH 100
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

#include <kernel.h>
#include <terminal.h>
#include <libc.h>
#include <screen.h>
#include <colors.h>
#include <keyboard.h>
#include <pcb.h>
#include <rtc.h>
#include <timer.h>
#include <ksyms.h>
#include <scheduler.h>

struct point {
    int x, y;
};

struct point snake[SNAKE_LENGTH];
int length = 5;
int speed = 100;
struct point fruit;
int direction = ARROW_RIGHT;

void init_game() {
    for (int i = 0; i < length; i++) {
        snake[i].x = 10 - i;
        snake[i].y = 10;
    }

    fruit.x = 20;
    fruit.y = 15;

    speed = 100;
}

void draw() {
    scr_clear();

    for(int i = 0; i < SCREEN_WIDTH; i++) {
        scrput(i, 0, ' ', VGA_COLOR_BLUE | VGA_COLOR_LIGHT_GREY << 4 );
        scrput(i, SCREEN_HEIGHT-1, ' ', VGA_COLOR_BLUE | VGA_COLOR_LIGHT_GREY << 4);
    }

    scrwrite(10, 0, "Snake Game - Use arrow keys to move", VGA_COLOR_BLUE | VGA_COLOR_LIGHT_GREY << 4);;
 
    for (int i = 1; i < length; i++) {
        scrput(snake[i].x, snake[i].y, 'a', VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4);
    }
    scrput(snake[0].x, snake[0].y, '@', VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4);

    scrput(fruit.x, fruit.y, 'F', VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4);

    scrprintf(0, SCREEN_HEIGHT-2, "Score: %d", length-5);
}

int get_input() {
    return scr_keyboard_get(0);
}

int update() {
    int input = get_input();
    if(input == CTRLC){
        dbgprintf("Exiting game\n");
        return -1;
    }
    if (input == ARROW_UP || input == ARROW_DOWN || input == ARROW_LEFT || input == ARROW_RIGHT) {
        direction = input; 
    }

    for (int i = length - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    if (direction == ARROW_UP) snake[0].y--;
    if (direction == ARROW_DOWN) snake[0].y++;
    if (direction == ARROW_LEFT) snake[0].x--;
    if (direction == ARROW_RIGHT) snake[0].x++;

    if (snake[0].x == fruit.x && snake[0].y == fruit.y) {
        length++;
        fruit.x = 2+ (rand() % (SCREEN_WIDTH-4));
        fruit.y = 2+ (rand() % (SCREEN_HEIGHT-4));
        speed -= 2;
    }

    /* Collision with walls */
    if (snake[0].x < 0 || snake[0].x >= SCREEN_WIDTH ||
        snake[0].y < 1 || snake[0].y >= SCREEN_HEIGHT-1) {
            dbgprintf("Collision with wall at %d, %d\n", snake[0].x, snake[0].y);
        return -1;
    }

    /* Self-collision */
    for (int i = 1; i < length; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            return -1;
        }
    }

    return 0;
}

void game_loop() {
    while (1) {
        draw();
        if(update() == -1) return;

        kernel_sleep(speed);
    }
}

static int snakegame() {
    init_game();
    game_loop();
    return 0;
}
EXPORT_KSYMBOL(snakegame);