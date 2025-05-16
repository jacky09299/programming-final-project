#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h> // For rand(), srand()
#include <time.h>   // For time()
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h> // Might need for future games

// Game Settings
const int SCREEN_W = 800; // Increased screen size
const int SCREEN_H = 600;
const char* FONT_PATH = "arial.ttf"; // Make sure this font exists
#define MAX_MENU_BUTTONS 4 // Max buttons on the menu

// Game States
typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_LEVEL_1,
    GAME_STATE_LEVEL_2,
    GAME_STATE_LEVEL_3
    // Add more states as you add levels
} GameState;

// Button Action (for menu buttons)
typedef enum {
    BUTTON_ACTION_NONE,
    BUTTON_ACTION_START_LEVEL_1,
    BUTTON_ACTION_START_LEVEL_2,
    BUTTON_ACTION_START_LEVEL_3,
    BUTTON_ACTION_QUIT
} MenuButtonAction;

// Button Structure (reused for menu)
typedef struct {
    int x, y, width, height;
    ALLEGRO_COLOR color;
    ALLEGRO_COLOR hover_color;
    ALLEGRO_COLOR click_color;
    const char* text;
    bool is_hovered;
    bool is_clicked;
    MenuButtonAction action; // Action for menu buttons
} Button;

// --- Level 1: Click the Target ---
typedef struct {
    int target_x, target_y, target_radius;
    int score;
    double time_left; // Optional: add a timer
    bool game_active;
} Level1Data;
Level1Data level1_data; // Global for simplicity, could be passed around

// --- Level 2: Reaction Timer ---
typedef enum {
    REACTION_STATE_WAITING,   // Waiting for the signal
    REACTION_STATE_SIGNAL,    // Signal is shown, waiting for click
    REACTION_STATE_TOO_EARLY, // Clicked before signal
    REACTION_STATE_RESULT     // Showing reaction time
} ReactionState;

typedef struct {
    ReactionState state;
    double wait_start_time;
    double signal_time_min; // Min time to wait for signal
    double signal_time_max; // Max time to wait for signal
    double current_wait_duration;
    double signal_shown_time;
    double reaction_time_ms;
} Level2Data;
Level2Data level2_data;

// --- Global Game Variables ---
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;
ALLEGRO_FONT* font_large = NULL;
ALLEGRO_FONT* font_medium = NULL;
ALLEGRO_FONT* font_small = NULL;

GameState current_game_state = GAME_STATE_MENU;
Button menu_buttons[MAX_MENU_BUTTONS];
int num_menu_buttons = 0;

// --- Function Prototypes ---
bool init_allegro_all();
void shutdown_allegro_all();

// Button functions
void init_menu_buttons();
void draw_button(Button* button, ALLEGRO_FONT* f);
bool is_mouse_over_button(int mouse_x, int mouse_y, Button* button);
void handle_menu_input(ALLEGRO_EVENT* ev, bool* running);
void draw_menu();

// Level 1 functions
void init_level_1();
void handle_input_level_1(ALLEGRO_EVENT* ev);
void update_level_1(double dt);
void draw_level_1();
void reset_target_level_1();

// Level 2 functions
void init_level_2();
void handle_input_level_2(ALLEGRO_EVENT* ev);
void update_level_2(double dt);
void draw_level_2();
void reset_reaction_game_level_2();

// Level 3 functions (placeholder)
void init_level_3();
void handle_input_level_3(ALLEGRO_EVENT* ev);
void update_level_3(double dt);
void draw_level_3();


// --- Main Function ---
int main() {
    if (!init_allegro_all()) {
        return -1;
    }

    srand((unsigned int)time(NULL)); // Seed random number generator

    init_menu_buttons(); // Initialize menu buttons first

    bool running = true;
    bool redraw = true;
    double last_time = al_get_time();

    al_start_timer(timer);

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);

        double current_time = al_get_time();
        double dt = current_time - last_time;
        last_time = current_time;

        // Global event handling
        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
        }
        if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                if (current_game_state != GAME_STATE_MENU) {
                    // Potentially cleanup current level before going to menu
                    if (current_game_state == GAME_STATE_LEVEL_2) {
                        // No specific cleanup needed now beyond re-init on entry
                    }
                    current_game_state = GAME_STATE_MENU;
                    // No need to re-init menu buttons, they are static
                }
                else {
                    running = false; // Exit from menu
                }
            }
        }

        // State-specific event handling
        switch (current_game_state) {
        case GAME_STATE_MENU:
            handle_menu_input(&ev, &running);
            break;
        case GAME_STATE_LEVEL_1:
            handle_input_level_1(&ev);
            break;
        case GAME_STATE_LEVEL_2:
            handle_input_level_2(&ev);
            break;
        case GAME_STATE_LEVEL_3:
            handle_input_level_3(&ev);
            break;
        }

        // State-specific updates (on timer event)
        if (ev.type == ALLEGRO_EVENT_TIMER) {
            switch (current_game_state) {
            case GAME_STATE_MENU:
                // No continuous update needed for menu
                break;
            case GAME_STATE_LEVEL_1:
                update_level_1(dt);
                break;
            case GAME_STATE_LEVEL_2:
                update_level_2(dt);
                break;
            case GAME_STATE_LEVEL_3:
                update_level_3(dt);
                break;
            }
            redraw = true;
        }

        // Drawing
        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(30, 30, 30)); // Dark background

            switch (current_game_state) {
            case GAME_STATE_MENU:
                draw_menu();
                break;
            case GAME_STATE_LEVEL_1:
                draw_level_1();
                break;
            case GAME_STATE_LEVEL_2:
                draw_level_2();
                break;
            case GAME_STATE_LEVEL_3:
                draw_level_3();
                break;
            }
            al_draw_text(font_small, al_map_rgb(200, 200, 200), SCREEN_W - 10, 10, ALLEGRO_ALIGN_RIGHT, "ESC to Menu/Quit");
            al_flip_display();
        }
    }

    shutdown_allegro_all();
    return 0;
}

// --- Allegro Initialization and Shutdown ---
bool init_allegro_all() {
    if (!al_init()) {
        fprintf(stderr, "Failed to initialize Allegro!\n");
        return false;
    }
    if (!al_install_keyboard()) {
        fprintf(stderr, "Failed to install keyboard!\n");
        return false;
    }
    if (!al_install_mouse()) {
        fprintf(stderr, "Failed to install mouse!\n");
        return false;
    }
    if (!al_init_primitives_addon()) {
        fprintf(stderr, "Failed to initialize primitives addon!\n");
        return false;
    }
    al_init_font_addon();
    if (!al_init_ttf_addon()) {
        fprintf(stderr, "Failed to initialize ttf addon!\n");
        return false;
    }
    if (!al_init_image_addon()) { // Good to have
        fprintf(stderr, "Failed to initialize image addon!\n");
        return false;
    }


    timer = al_create_timer(1.0 / 60.0); // 60 FPS
    if (!timer) {
        fprintf(stderr, "Failed to create timer!\n");
        return false;
    }

    display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) {
        fprintf(stderr, "Failed to create display!\n");
        al_destroy_timer(timer);
        return false;
    }
    al_set_window_title(display, "Multi-Level Mini-Games");

    font_large = al_load_ttf_font(FONT_PATH, 48, 0);
    font_medium = al_load_ttf_font(FONT_PATH, 28, 0);
    font_small = al_load_ttf_font(FONT_PATH, 18, 0);
    if (!font_large || !font_medium || !font_small) {
        fprintf(stderr, "Failed to load font '%s'. Make sure it's in the same directory or provide full path.\n", FONT_PATH);
        al_destroy_display(display);
        al_destroy_timer(timer);
        return false;
    }

    event_queue = al_create_event_queue();
    if (!event_queue) {
        fprintf(stderr, "Failed to create event queue!\n");
        al_destroy_font(font_large);
        al_destroy_font(font_medium);
        al_destroy_font(font_small);
        al_destroy_display(display);
        al_destroy_timer(timer);
        return false;
    }

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_mouse_event_source());

    return true;
}

void shutdown_allegro_all() {
    if (font_large) al_destroy_font(font_large);
    if (font_medium) al_destroy_font(font_medium);
    if (font_small) al_destroy_font(font_small);
    if (timer) al_destroy_timer(timer);
    if (event_queue) al_destroy_event_queue(event_queue);
    if (display) al_destroy_display(display);

    // Shutdown addons in reverse order of init if necessary, though for these it's usually fine.
    al_shutdown_ttf_addon();
    al_shutdown_font_addon();
    al_shutdown_primitives_addon();
    al_uninstall_mouse();
    al_uninstall_keyboard();
    al_shutdown_image_addon();
    // al_uninstall_system(); // al_init implicitly calls this.
}

// --- Button Helper Functions (Similar to your original) ---
void draw_button(Button* button, ALLEGRO_FONT* f) {
    ALLEGRO_COLOR current_color = button->color;
    if (button->is_clicked) {
        current_color = button->click_color;
    }
    else if (button->is_hovered) {
        current_color = button->hover_color;
    }

    al_draw_filled_rounded_rectangle(button->x, button->y,
        button->x + button->width, button->y + button->height,
        10, 10, current_color); // Rounded corners

    al_draw_rounded_rectangle(button->x, button->y,
        button->x + button->width, button->y + button->height,
        10, 10, al_map_rgb(200, 200, 200), 2); // Lighter border

    if (button->text && f) {
        float text_width = al_get_text_width(f, button->text);
        float text_height = al_get_font_line_height(f);
        al_draw_text(f, al_map_rgb(255, 255, 255),
            button->x + button->width / 2.0f,
            button->y + button->height / 2.0f - text_height / 2.0f,
            ALLEGRO_ALIGN_CENTER, button->text);
    }
}

bool is_mouse_over_button(int mouse_x, int mouse_y, Button* button) {
    return (mouse_x >= button->x &&
        mouse_x <= button->x + button->width &&
        mouse_y >= button->y &&
        mouse_y <= button->y + button->height);
}

// --- Menu Functions ---
void init_menu_buttons() {
    num_menu_buttons = 0;
    int btn_w = 250;
    int btn_h = 60;
    int spacing = 20;
    int start_y = SCREEN_H / 2 - ((3 * (btn_h + spacing) - spacing) / 2); // For 3 main level buttons

    menu_buttons[num_menu_buttons++] = (Button){
        SCREEN_W / 2 - btn_w / 2, start_y, btn_w, btn_h,
        al_map_rgb(50, 150, 50), al_map_rgb(70, 200, 70), al_map_rgb(30, 100, 30),
        "Level 1: Click Target", false, false, BUTTON_ACTION_START_LEVEL_1
    };
    menu_buttons[num_menu_buttons++] = (Button){
        SCREEN_W / 2 - btn_w / 2, start_y + btn_h + spacing, btn_w, btn_h,
        al_map_rgb(50, 50, 150), al_map_rgb(70, 70, 200), al_map_rgb(30, 30, 100),
        "Level 2: Reaction Test", false, false, BUTTON_ACTION_START_LEVEL_2
    };
    menu_buttons[num_menu_buttons++] = (Button){
        SCREEN_W / 2 - btn_w / 2, start_y + 2 * (btn_h + spacing), btn_w, btn_h,
        al_map_rgb(150, 50, 50), al_map_rgb(200, 70, 70), al_map_rgb(100, 30, 30),
        "Level 3 (Placeholder)", false, false, BUTTON_ACTION_START_LEVEL_3
    };
    menu_buttons[num_menu_buttons++] = (Button){ // Quit button
       SCREEN_W / 2 - btn_w / 2, start_y + 3 * (btn_h + spacing) + spacing, btn_w, btn_h,
       al_map_rgb(100, 100, 100), al_map_rgb(150, 150, 150), al_map_rgb(50, 50, 50),
       "Quit Game", false, false, BUTTON_ACTION_QUIT
    };
}

void handle_menu_input(ALLEGRO_EVENT* ev, bool* running) {
    if (ev->type == ALLEGRO_EVENT_MOUSE_AXES || ev->type == ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY) {
        for (int i = 0; i < num_menu_buttons; ++i) {
            menu_buttons[i].is_hovered = is_mouse_over_button(ev->mouse.x, ev->mouse.y, &menu_buttons[i]);
        }
    }
    else if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->mouse.button & 1) { // Left click
            for (int i = 0; i < num_menu_buttons; ++i) {
                if (is_mouse_over_button(ev->mouse.x, ev->mouse.y, &menu_buttons[i])) {
                    menu_buttons[i].is_clicked = true;
                }
            }
        }
    }
    else if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
        if (ev->mouse.button & 1) { // Left release
            for (int i = 0; i < num_menu_buttons; ++i) {
                if (menu_buttons[i].is_clicked && is_mouse_over_button(ev->mouse.x, ev->mouse.y, &menu_buttons[i])) {
                    switch (menu_buttons[i].action) {
                    case BUTTON_ACTION_START_LEVEL_1:
                        current_game_state = GAME_STATE_LEVEL_1;
                        init_level_1();
                        break;
                    case BUTTON_ACTION_START_LEVEL_2:
                        current_game_state = GAME_STATE_LEVEL_2;
                        init_level_2();
                        break;
                    case BUTTON_ACTION_START_LEVEL_3:
                        current_game_state = GAME_STATE_LEVEL_3;
                        init_level_3();
                        break;
                    case BUTTON_ACTION_QUIT:
                        *running = false;
                        break;
                    case BUTTON_ACTION_NONE: // Should not happen with proper init
                        break;
                    }
                }
                menu_buttons[i].is_clicked = false;
            }
        }
    }
}

void draw_menu() {
    al_draw_text(font_large, al_map_rgb(220, 220, 220), SCREEN_W / 2, 80, ALLEGRO_ALIGN_CENTER, "Mini-Game Collection");
    for (int i = 0; i < num_menu_buttons; ++i) {
        draw_button(&menu_buttons[i], font_medium);
    }
}


// --- Level 1: Click the Target ---
void init_level_1() {
    level1_data.score = 0;
    level1_data.game_active = true;
    level1_data.time_left = 30.0; // 30 seconds per game
    reset_target_level_1();
    printf("Level 1 Initialized\n");
}

void reset_target_level_1() {
    level1_data.target_radius = 20 + rand() % 30; // Radius between 20 and 49
    level1_data.target_x = level1_data.target_radius + rand() % (SCREEN_W - 2 * level1_data.target_radius);
    level1_data.target_y = level1_data.target_radius + rand() % (SCREEN_H - 2 * level1_data.target_radius - 50) + 50; // Keep away from top score text
}

void handle_input_level_1(ALLEGRO_EVENT* ev) {
    if (!level1_data.game_active) {
        if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) { // Click to restart or go to menu
            init_level_1(); // Restart
        }
        return;
    }

    if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->mouse.button & 1) {
            int mx = ev->mouse.x;
            int my = ev->mouse.y;
            // Check click on target (Pythagorean theorem for circle)
            int dx = mx - level1_data.target_x;
            int dy = my - level1_data.target_y;
            if (dx * dx + dy * dy < level1_data.target_radius * level1_data.target_radius) {
                level1_data.score++;
                reset_target_level_1();
            }
        }
    }
}

void update_level_1(double dt) {
    if (!level1_data.game_active) return;

    level1_data.time_left -= dt;
    if (level1_data.time_left <= 0) {
        level1_data.time_left = 0;
        level1_data.game_active = false;
    }
}

void draw_level_1() {
    char text_buffer[100];

    // Draw score and time
    sprintf(text_buffer, "Score: %d", level1_data.score);
    al_draw_text(font_medium, al_map_rgb(255, 255, 255), 20, 20, 0, text_buffer);
    sprintf(text_buffer, "Time: %.1f", level1_data.time_left > 0 ? level1_data.time_left : 0.0);
    al_draw_text(font_medium, al_map_rgb(255, 255, 255), SCREEN_W - 20, 20, ALLEGRO_ALIGN_RIGHT, text_buffer);


    if (level1_data.game_active) {
        // Draw target
        al_draw_filled_circle(level1_data.target_x, level1_data.target_y, level1_data.target_radius, al_map_rgb(255, 0, 0));
        al_draw_circle(level1_data.target_x, level1_data.target_y, level1_data.target_radius, al_map_rgb(255, 255, 255), 2);
    }
    else {
        // Game Over message
        al_draw_text(font_large, al_map_rgb(255, 200, 0), SCREEN_W / 2, SCREEN_H / 2 - 50, ALLEGRO_ALIGN_CENTER, "Game Over!");
        sprintf(text_buffer, "Final Score: %d", level1_data.score);
        al_draw_text(font_medium, al_map_rgb(255, 255, 255), SCREEN_W / 2, SCREEN_H / 2 + 10, ALLEGRO_ALIGN_CENTER, text_buffer);
        al_draw_text(font_medium, al_map_rgb(200, 200, 200), SCREEN_W / 2, SCREEN_H / 2 + 50, ALLEGRO_ALIGN_CENTER, "Click to Retry");
    }
}

// --- Level 2: Reaction Timer ---
void init_level_2() {
    level2_data.signal_time_min = 1.5; // seconds
    level2_data.signal_time_max = 5.0; // seconds
    reset_reaction_game_level_2();
    printf("Level 2 Initialized\n");
}

void reset_reaction_game_level_2() {
    level2_data.state = REACTION_STATE_WAITING;
    level2_data.wait_start_time = al_get_time();
    // Random time between min and max
    level2_data.current_wait_duration = level2_data.signal_time_min +
        (double)rand() / RAND_MAX * (level2_data.signal_time_max - level2_data.signal_time_min);
    level2_data.reaction_time_ms = 0;
}

void handle_input_level_2(ALLEGRO_EVENT* ev) {
    if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->mouse.button & 1) {
            if (level2_data.state == REACTION_STATE_WAITING) {
                level2_data.state = REACTION_STATE_TOO_EARLY;
            }
            else if (level2_data.state == REACTION_STATE_SIGNAL) {
                level2_data.reaction_time_ms = (al_get_time() - level2_data.signal_shown_time) * 1000.0;
                level2_data.state = REACTION_STATE_RESULT;
            }
            else if (level2_data.state == REACTION_STATE_TOO_EARLY || level2_data.state == REACTION_STATE_RESULT) {
                reset_reaction_game_level_2(); // Click to try again
            }
        }
    }
}

void update_level_2(double dt) {
    if (level2_data.state == REACTION_STATE_WAITING) {
        if (al_get_time() - level2_data.wait_start_time >= level2_data.current_wait_duration) {
            level2_data.state = REACTION_STATE_SIGNAL;
            level2_data.signal_shown_time = al_get_time();
        }
    }
}

void draw_level_2() {
    char text_buffer[100];
    ALLEGRO_COLOR bg_color = al_map_rgb(50, 50, 150); // Default blueish

    al_draw_text(font_medium, al_map_rgb(255, 255, 255), SCREEN_W / 2, 50, ALLEGRO_ALIGN_CENTER, "Reaction Test");

    switch (level2_data.state) {
    case REACTION_STATE_WAITING:
        bg_color = al_map_rgb(180, 0, 0); // Red
        al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, bg_color); // Redraw background to cover previous
        al_draw_text(font_large, al_map_rgb(255, 255, 255), SCREEN_W / 2, SCREEN_H / 2 - 30, ALLEGRO_ALIGN_CENTER, "Wait for Green...");
        al_draw_text(font_medium, al_map_rgb(220, 220, 220), SCREEN_W / 2, SCREEN_H / 2 + 40, ALLEGRO_ALIGN_CENTER, "Click when the screen turns green.");
        break;
    case REACTION_STATE_SIGNAL:
        bg_color = al_map_rgb(0, 180, 0); // Green
        al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, bg_color);
        al_draw_text(font_large, al_map_rgb(255, 255, 255), SCREEN_W / 2, SCREEN_H / 2 - 30, ALLEGRO_ALIGN_CENTER, "CLICK NOW!");
        break;
    case REACTION_STATE_TOO_EARLY:
        al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgb(30, 30, 30)); // Reset to dark
        al_draw_text(font_large, al_map_rgb(255, 100, 0), SCREEN_W / 2, SCREEN_H / 2 - 30, ALLEGRO_ALIGN_CENTER, "Too Early!");
        al_draw_text(font_medium, al_map_rgb(200, 200, 200), SCREEN_W / 2, SCREEN_H / 2 + 40, ALLEGRO_ALIGN_CENTER, "Click to try again.");
        break;
    case REACTION_STATE_RESULT:
        al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgb(30, 30, 30)); // Reset to dark
        sprintf(text_buffer, "Your Reaction: %.0f ms", level2_data.reaction_time_ms);
        al_draw_text(font_large, al_map_rgb(100, 200, 255), SCREEN_W / 2, SCREEN_H / 2 - 30, ALLEGRO_ALIGN_CENTER, text_buffer);
        al_draw_text(font_medium, al_map_rgb(200, 200, 200), SCREEN_W / 2, SCREEN_H / 2 + 40, ALLEGRO_ALIGN_CENTER, "Click to try again.");
        break;
    }
    // Redraw title as filled_rectangle might cover it
    al_draw_text(font_medium, al_map_rgb(255, 255, 255), SCREEN_W / 2, 50, ALLEGRO_ALIGN_CENTER, "Reaction Test");
    al_draw_text(font_small, al_map_rgb(200, 200, 200), SCREEN_W - 10, 10, ALLEGRO_ALIGN_RIGHT, "ESC to Menu/Quit"); // Ensure this is always visible
}


// --- Level 3: Placeholder ---
void init_level_3() {
    printf("Level 3 Initialized (Placeholder)\n");
}
void handle_input_level_3(ALLEGRO_EVENT* ev) {
    // No specific input yet
}
void update_level_3(double dt) {
    // No specific update yet
}
void draw_level_3() {
    al_draw_text(font_large, al_map_rgb(200, 200, 0), SCREEN_W / 2, SCREEN_H / 2 - 30, ALLEGRO_ALIGN_CENTER, "Level 3");
    al_draw_text(font_medium, al_map_rgb(220, 220, 220), SCREEN_W / 2, SCREEN_H / 2 + 30, ALLEGRO_ALIGN_CENTER, "Coming Soon!");
}