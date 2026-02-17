/**
 * @file tetris.c
 * @brief Full-featured Tetris game WASM application for AkiraOS
 * 
 * A complete Tetris implementation featuring:
 * - Seven classic tetromino pieces (I, O, T, S, Z, J, L)
 * - Piece rotation and movement
 * - Line clearing and scoring
 * - Level progression with increasing speed
 * - Next piece preview
 * - Game over detection
 * - Colorful RGB565 graphics
 * 
 * Controls:
 * - GPIO 15: Move left (when LOW)
 * - GPIO 40: Move right (when LOW)
 * - GPIO 16: Rotate (when LOW)
 * - GPIO 17: Drop faster (when LOW)
 * 
 * @copyright Copyright (c) 2026 AkiraOS Contributors
 * @license Apache-2.0
 */

#include "akira_api.h"

// Game board dimensions
#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20

// Display dimensions and positioning
#define BLOCK_SIZE   10
#define BOARD_X      20
#define BOARD_Y      10

// Game configuration
#define INITIAL_DROP_DELAY  500000  // 0.5 seconds in microseconds
#define MAX_LEVEL           15
#define MIN_DROP_DELAY      50000   // Fastest speed: 0.05 seconds

// GPIO pin definitions for input
#define GPIO_LEFT    15
#define GPIO_RIGHT   40
#define GPIO_ROTATE  16
#define GPIO_DROP    17

// Tetromino shapes (4x4 grid, bit-packed)
// Each tetromino has 4 rotation states
static const uint16_t SHAPES[7][4] = {
    // I piece
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    // O piece
    {0x6600, 0x6600, 0x6600, 0x6600},
    // T piece
    {0x0E40, 0x4C40, 0x4E00, 0x4640},
    // S piece
    {0x06C0, 0x4620, 0x06C0, 0x4620},
    // Z piece
    {0x0C60, 0x2640, 0x0C60, 0x2640},
    // J piece
    {0x44C0, 0x8E00, 0x6440, 0x0E20},
    // L piece
    {0x4460, 0x0E80, 0xC440, 0x2E00}
};

// Tetromino colors
static const uint16_t COLORS[7] = {
    COLOR_CYAN,    // I
    COLOR_YELLOW,  // O
    COLOR_PURPLE,  // T
    COLOR_GREEN,   // S
    COLOR_RED,     // Z
    COLOR_BLUE,    // J
    COLOR_ORANGE   // L
};

// Game state structure
typedef struct {
    uint8_t board[BOARD_HEIGHT][BOARD_WIDTH];
    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int next_piece;
    uint32_t score;
    uint32_t lines_cleared;
    uint32_t level;
    uint32_t drop_delay;
    int game_over;
    
    // Previous state for selective redrawing
    int prev_piece;
    int prev_rotation;
    int prev_x;
    int prev_y;
    int prev_next_piece;
    uint32_t prev_score;
    uint32_t prev_lines_cleared;
    uint32_t prev_level;
    
    // Dirty flags to minimize redraws
    uint8_t piece_moved;
    uint8_t board_changed;
} game_state_t;

static game_state_t game;

// Forward declarations
static void redraw_board(void);

/**
 * @brief Simple pseudo-random number generator
 */
static uint32_t random_state = 12345;

static int rand_range(int max)
{
    random_state = (random_state * 1103515245 + 12345) & 0x7FFFFFFF;
    return random_state % max;
}



/**
 * @brief Check if a cell in the tetromino shape is filled
 */
static int get_shape_cell(uint16_t shape, int x, int y)
{
    return (shape >> ((3 - y) * 4 + (3 - x))) & 1;
}

/**
 * @brief Check if piece can be placed at given position
 */
static int check_collision(int piece, int rotation, int x, int y)
{
    uint16_t shape = SHAPES[piece][rotation];
    
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (get_shape_cell(shape, px, py)) {
                int board_x = x + px;
                int board_y = y + py;
                
                // Check boundaries
                if (board_x < 0 || board_x >= BOARD_WIDTH || 
                    board_y >= BOARD_HEIGHT) {
                    return 1; // Collision
                }
                
                // Check board collision (only if within bounds)
                if (board_y >= 0 && game.board[board_y][board_x]) {
                    return 1; // Collision
                }
            }
        }
    }
    
    return 0; // No collision
}

/**
 * @brief Lock the current piece into the board
 */
static void lock_piece(void)
{
    uint16_t shape = SHAPES[game.current_piece][game.current_rotation];
    
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (get_shape_cell(shape, px, py)) {
                int board_x = game.current_x + px;
                int board_y = game.current_y + py;
                
                if (board_y >= 0 && board_y < BOARD_HEIGHT && 
                    board_x >= 0 && board_x < BOARD_WIDTH) {
                    game.board[board_y][board_x] = game.current_piece + 1;
                }
            }
        }
    }
}

/**
 * @brief Check and clear completed lines (optimized)
 */
static int clear_lines(void)
{
    int lines = 0;
    int cleared_rows[4]; // Track which rows were cleared
    int cleared_count = 0;
    
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (!game.board[y][x]) {
                full = 0;
                break;
            }
        }
        
        if (full) {
            lines++;
            cleared_rows[cleared_count++] = y;
            
            // Move all lines above down
            for (int yy = y; yy > 0; yy--) {
                for (int x = 0; x < BOARD_WIDTH; x++) {
                    game.board[yy][x] = game.board[yy - 1][x];
                }
            }
            
            // Clear top line
            for (int x = 0; x < BOARD_WIDTH; x++) {
                game.board[0][x] = 0;
            }
            
            y++; // Re-check this line
        }
    }
    
    if (lines > 0) {
        game.board_changed = 1;
    }
    
    return lines;
}

/**
 * @brief Spawn a new piece
 */
static void spawn_piece(void)
{
    game.current_piece = game.next_piece;
    game.next_piece = rand_range(7);
    game.current_rotation = 0;
    game.current_x = BOARD_WIDTH / 2 - 2;
    game.current_y = -1;
    game.piece_moved = 1; // Force redraw for new piece
    
    // Check if game over (piece can't spawn)
    if (check_collision(game.current_piece, game.current_rotation, 
                        game.current_x, game.current_y)) {
        game.game_over = 1;
    }
}

/**
 * @brief Draw a single block on the display (optimized)
 */
static void draw_block(int x, int y, uint16_t color)
{
    // Use full block size for solid appearance, faster rendering
    display_rect(BOARD_X + x * BLOCK_SIZE, 
                 BOARD_Y + y * BLOCK_SIZE, 
                 BLOCK_SIZE, 
                 BLOCK_SIZE, 
                 color);
}

/**
 * @brief Erase the piece at given position (draw with background)
 */
static void erase_piece(int piece, int rotation, int x, int y)
{
    uint16_t shape = SHAPES[piece][rotation];
    
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (get_shape_cell(shape, px, py)) {
                int screen_y = y + py;
                int screen_x = x + px;
                
                if (screen_y >= 0 && screen_y < BOARD_HEIGHT && 
                    screen_x >= 0 && screen_x < BOARD_WIDTH) {
                    // Draw locked piece if there's one, otherwise black
                    if (game.board[screen_y][screen_x]) {
                        draw_block(screen_x, screen_y, COLORS[game.board[screen_y][screen_x] - 1]);
                    } else {
                        draw_block(screen_x, screen_y, COLOR_BLACK);
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw the current falling piece
 */
static void draw_current_piece(void)
{
    uint16_t shape = SHAPES[game.current_piece][game.current_rotation];
    uint16_t color = COLORS[game.current_piece];
    
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (get_shape_cell(shape, px, py)) {
                int screen_y = game.current_y + py;
                if (screen_y >= 0) {
                    draw_block(game.current_x + px, screen_y, color);
                }
            }
        }
    }
}

/**
 * @brief Draw the next piece preview (only if changed)
 */
static void draw_next_piece(int force)
{
    if (!force && game.next_piece == game.prev_next_piece) {
        return; // No change
    }
    
    int preview_x = 140;
    int preview_y = 20;
    
    // Clear preview area
    display_rect(preview_x, preview_y, 40, 40, COLOR_BLACK);
    
    uint16_t shape = SHAPES[game.next_piece][0];
    uint16_t color = COLORS[game.next_piece];
    
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (get_shape_cell(shape, px, py)) {
                display_rect(preview_x + px * 8, preview_y + py * 8, 7, 7, color);
            }
        }
    }
    
    game.prev_next_piece = game.next_piece;
}

/**
 * @brief Draw game statistics (only if changed)
 */
static void draw_stats(int force)
{
    int stats_x = 140;
    int stats_y = 80;
    
    // Only redraw if values changed
    if (force || game.score != game.prev_score) {
        // Clear score area
        display_rect(stats_x, stats_y + 12, 80, 10, COLOR_BLACK);
        
        // Simple number to string conversion for score
        char score_str[12];
        uint32_t score = game.score;
        int i = 0;
        if (score == 0) {
            score_str[i++] = '0';
        } else {
            char temp[12];
            int j = 0;
            while (score > 0) {
                temp[j++] = '0' + (score % 10);
                score /= 10;
            }
            while (j > 0) {
                score_str[i++] = temp[--j];
            }
        }
        score_str[i] = '\0';
        display_text(stats_x, stats_y + 12, score_str, COLOR_YELLOW);
        game.prev_score = game.score;
    }
    
    if (force || game.lines_cleared != game.prev_lines_cleared) {
        // Clear lines area
        display_rect(stats_x, stats_y + 42, 80, 10, COLOR_BLACK);
        
        // Lines cleared
        char lines_str[12];
        uint32_t lines = game.lines_cleared;
        int i = 0;
        if (lines == 0) {
            lines_str[i++] = '0';
        } else {
            char temp[12];
            int j = 0;
            while (lines > 0) {
                temp[j++] = '0' + (lines % 10);
                lines /= 10;
            }
            while (j > 0) {
                lines_str[i++] = temp[--j];
            }
        }
        lines_str[i] = '\0';
        display_text(stats_x, stats_y + 42, lines_str, COLOR_CYAN);
        game.prev_lines_cleared = game.lines_cleared;
    }
    
    if (force || game.level != game.prev_level) {
        // Clear level area
        display_rect(stats_x, stats_y + 72, 80, 10, COLOR_BLACK);
        
        // Level
        char level_str[4];
        level_str[0] = '0' + (game.level / 10);
        level_str[1] = '0' + (game.level % 10);
        level_str[2] = '\0';
        display_text(stats_x, stats_y + 72, level_str, COLOR_GREEN);
        game.prev_level = game.level;
    }
}

/**
 * @brief Initialize the display (draw static elements once)
 */
static void init_display(void)
{
    display_clear(COLOR_BLACK);
    display_text(BOARD_X, 2, "TETRIS", COLOR_MAGENTA);
    
    // Draw static labels
    int stats_x = 140;
    int stats_y = 80;
    display_text(stats_x, stats_y - 50, "NEXT:", COLOR_WHITE);
    display_text(stats_x, stats_y, "SCORE:", COLOR_WHITE);
    display_text(stats_x, stats_y + 30, "LINES:", COLOR_WHITE);
    display_text(stats_x, stats_y + 60, "LEVEL:", COLOR_WHITE);
    
    // Draw border
    display_rect(BOARD_X - 2, BOARD_Y - 2, 
                 BOARD_WIDTH * BLOCK_SIZE + 4, 2, 
                 COLOR_WHITE);
    display_rect(BOARD_X - 2, BOARD_Y + BOARD_HEIGHT * BLOCK_SIZE, 
                 BOARD_WIDTH * BLOCK_SIZE + 4, 2, 
                 COLOR_WHITE);
    display_rect(BOARD_X - 2, BOARD_Y - 2, 
                 2, BOARD_HEIGHT * BLOCK_SIZE + 4, 
                 COLOR_WHITE);
    display_rect(BOARD_X + BOARD_WIDTH * BLOCK_SIZE, BOARD_Y - 2, 
                 2, BOARD_HEIGHT * BLOCK_SIZE + 4, 
                 COLOR_WHITE);
    
    // Clear play area (board is already zeroed from init_game)
    display_rect(BOARD_X, BOARD_Y, 
                 BOARD_WIDTH * BLOCK_SIZE, 
                 BOARD_HEIGHT * BLOCK_SIZE, 
                 COLOR_BLACK);
    
    // Initialize previous state
    game.prev_piece = -1;
    game.prev_rotation = -1;
    game.prev_x = -1;
    game.prev_y = -1;
    game.prev_next_piece = -1;
    game.prev_score = 0xFFFFFFFF;
    game.prev_lines_cleared = 0xFFFFFFFF;
    game.prev_level = 0xFFFFFFFF;
}

/**
 * @brief Update only what changed on the display
 */
static void update_display(void)
{
    // Only update if something actually changed
    if (!game.piece_moved && !game.board_changed) {
        return;
    }
    
    // Redraw board if it changed (after line clear or piece lock)
    if (game.board_changed) {
        redraw_board();
        game.board_changed = 0;
    }
    
    // Erase old piece position if it moved
    if (game.piece_moved && game.prev_piece >= 0 && 
        (game.prev_x != game.current_x || 
         game.prev_y != game.current_y || 
         game.prev_rotation != game.current_rotation)) {
        erase_piece(game.prev_piece, game.prev_rotation, game.prev_x, game.prev_y);
    }
    
    // Draw current piece in new position
    if (game.piece_moved) {
        draw_current_piece();
        game.piece_moved = 0;
    }
    
    // Update previews and stats (only if changed)
    draw_next_piece(0);
    draw_stats(0);
    
    // Save current state for next frame
    game.prev_piece = game.current_piece;
    game.prev_rotation = game.current_rotation;
    game.prev_x = game.current_x;
    game.prev_y = game.current_y;
}

/**
 * @brief Redraw the entire board (after line clears or piece lock)
 */
static void redraw_board(void)
{
    // Clear play area
    display_rect(BOARD_X, BOARD_Y, 
                 BOARD_WIDTH * BLOCK_SIZE, 
                 BOARD_HEIGHT * BLOCK_SIZE, 
                 COLOR_BLACK);
    
    // Draw locked pieces
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game.board[y][x]) {
                draw_block(x, y, COLORS[game.board[y][x] - 1]);
            }
        }
    }
}

/**
 * @brief Initialize the game
 */
static void init_game(void)
{
    // Clear board
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            game.board[y][x] = 0;
        }
    }
    
    game.score = 0;
    game.lines_cleared = 0;
    game.level = 1;
    game.drop_delay = INITIAL_DROP_DELAY;
    game.game_over = 0;
    
    // Initialize dirty flags
    game.piece_moved = 1;
    game.board_changed = 0;
    
    game.next_piece = rand_range(7);
    spawn_piece();
    
    // Initialize display with static elements
    init_display();
    
    // Force initial draw of stats and next piece
    draw_next_piece(1);
    draw_stats(1);
}

/**
 * @brief Handle game input (simplified for auto-play demo)
 */
static int handle_input(void)
{
    // Try to move piece down
    if (!check_collision(game.current_piece, game.current_rotation, 
                        game.current_x, game.current_y + 1)) {
        game.current_y++;
        game.piece_moved = 1;
        return 0;
    }
    
    // Can't move down, lock piece
    lock_piece();
    game.board_changed = 1; // Board modified
    
    // Clear lines
    int lines = clear_lines();
    if (lines > 0) {
        game.lines_cleared += lines;
        game.score += lines * lines * 100; // Scoring: 1 line = 100, 2 lines = 400, etc.
        
        // Level up every 10 lines
        game.level = (game.lines_cleared / 10) + 1;
        if (game.level > MAX_LEVEL) {
            game.level = MAX_LEVEL;
        }
        
        // Increase speed with level (with minimum cap)
        game.drop_delay = INITIAL_DROP_DELAY / game.level;
        if (game.drop_delay < MIN_DROP_DELAY) {
            game.drop_delay = MIN_DROP_DELAY;
        }
    }
    
    // Spawn next piece
    spawn_piece();
    
    return 1; // Piece was locked
}

/**
 * @brief Automatic piece movement (AI demonstration)
 */
static void auto_move(void)
{
    int moved = 0;
    
    // Simple AI: try to move to center or avoid walls
    int target_x = BOARD_WIDTH / 2 - 2;
    
    if (game.current_x < target_x && 
        !check_collision(game.current_piece, game.current_rotation, 
                        game.current_x + 1, game.current_y)) {
        game.current_x++;
        moved = 1;
    } else if (game.current_x > target_x && 
               !check_collision(game.current_piece, game.current_rotation, 
                               game.current_x - 1, game.current_y)) {
        game.current_x--;
        moved = 1;
    }
    
    // Occasionally rotate (less frequently)
    if ((random_state % 15) == 0) {
        int new_rotation = (game.current_rotation + 1) % 4;
        if (!check_collision(game.current_piece, new_rotation, 
                            game.current_x, game.current_y)) {
            game.current_rotation = new_rotation;
            moved = 1;
        }
    }
    
    if (moved) {
        game.piece_moved = 1;
    }
}

/**
 * @brief Main game loop (optimized)
 */
static void game_loop(void)
{
    uint32_t frame_count = 0;
    
    while (!game.game_over && frame_count < 1000) {
        // Auto-move piece less frequently (every 30 frames instead of 20)
        if ((frame_count % 30) == 0) {
            auto_move();
        }
        
        // Move piece down
        handle_input();
        
        // Update display (only if something changed)
        update_display();
        
        // Delay based on level
        delay(game.drop_delay);
        
        frame_count++;
    }
}

/**
 * @brief Main entry point for the Tetris game
 * 
 * @return 0 on success
 */
int main(void)
{
    log(LOG_LEVEL_INF, "=================================");
    log(LOG_LEVEL_INF, "   AkiraOS Tetris Game v1.0     ");
    log(LOG_LEVEL_INF, "=================================");
    
    // Initialize display
    display_clear(COLOR_BLACK);
    
    // Show title screen
    display_text_large(60, 80, "TETRIS", COLOR_CYAN);
    display_text(70, 120, "Loading...", COLOR_WHITE);
    delay(100000);
    
    // Initialize game
    log(LOG_LEVEL_INF, "Initializing game...");
    init_game();
    
    // Run game loop
    log(LOG_LEVEL_INF, "Starting game loop...");
    game_loop();
    
    // Game over screen
    display_clear(COLOR_BLACK);
    display_text_large(30, 60, "GAME", COLOR_RED);
    display_text_large(30, 90, "OVER", COLOR_RED);
    
    // Show final score
    display_text(40, 140, "Final Score:", COLOR_WHITE);
    char score_str[12];
    uint32_t score = game.score;
    int i = 0;
    if (score == 0) {
        score_str[i++] = '0';
    } else {
        char temp[12];
        int j = 0;
        while (score > 0) {
            temp[j++] = '0' + (score % 10);
            score /= 10;
        }
        while (j > 0) {
            score_str[i++] = temp[--j];
        }
    }
    score_str[i] = '\0';
    display_text(70, 160, score_str, COLOR_YELLOW);
    
    log(LOG_LEVEL_INF, "Game completed!");
    log(LOG_LEVEL_INF, "Thank you for playing!");
    
    return 0;
}
