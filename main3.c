#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_native_dialog.h>
#include <stdio.h>
#include <stdlib.h> // For rand() and srand()
#include <time.h>   // For time()
#include <math.h>   // For sqrt() or pow() if needed (not strictly for this example)

// --- 常數定義 ---
const int SCREEN_W = 800;
const int SCREEN_H = 600;
const float FPS = 60.0;
const int TARGET_RADIUS = 25;
const char* FONT_FILE = "arial.ttf"; // 字體文件，請確保此文件存在

// --- 全局變數 ---
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;
ALLEGRO_FONT* font = NULL;

int target_x, target_y;
int score = 0;
int mouse_x, mouse_y; // 當前鼠標位置

// --- 函數原型 ---
void init_allegro();
void shutdown_allegro();
void spawn_new_target();
bool is_click_on_target(int click_x, int click_y);

// --- 主函數 ---
int main(int argc, char** argv) {
    init_allegro();

    // 初始化隨機數生成器
    srand(time(NULL));

    // 產生第一個目標
    spawn_new_target();

    bool running = true;
    bool redraw = true;

    al_start_timer(timer);

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);

        if (ev.type == ALLEGRO_EVENT_TIMER) {
            redraw = true;
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                running = false;
            }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
            mouse_x = ev.mouse.x;
            mouse_y = ev.mouse.y;
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev.mouse.button == 1) { // 左鍵點擊
                if (is_click_on_target(ev.mouse.x, ev.mouse.y)) {
                    score++;
                    spawn_new_target();
                }
            }
        }

        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(30, 30, 30)); // 深灰色背景

            // 繪製目標
            al_draw_filled_circle(target_x, target_y, TARGET_RADIUS, al_map_rgb(255, 0, 0)); // 紅色目標

            // 繪製分數
            al_draw_textf(font, al_map_rgb(255, 255, 255), 10, 10, 0, "Score: %d", score);

            // (可選) 繪製一個簡單的鼠標指針，如果你隱藏了系統鼠標
            // al_draw_filled_circle(mouse_x, mouse_y, 5, al_map_rgb(0, 255, 0));

            al_flip_display();
        }
    }

    shutdown_allegro();
    return 0;
}

// --- 初始化 Allegro ---
void init_allegro() {
    if (!al_init()) {
        al_show_native_message_box(NULL, "Error", "Allegro Error",
            "Failed to initialize allegro!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }

    if (!al_init_primitives_addon()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error",
            "Failed to initialize primitives addon!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }

    if (!al_install_mouse()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error",
            "Failed to install mouse!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }

    if (!al_install_keyboard()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error",
            "Failed to install keyboard!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }

    al_init_font_addon(); // 初始化字體插件
    if (!al_init_ttf_addon()) { // 初始化 TTF 字體插件
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error",
            "Failed to initialize ttf addon!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }

    display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) {
        al_show_native_message_box(NULL, "Error", "Display Error",
            "Failed to create display!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    al_set_window_title(display, "Click The Target Game");

    event_queue = al_create_event_queue();
    if (!event_queue) {
        al_show_native_message_box(NULL, "Error", "Event Queue Error",
            "Failed to create event_queue!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        al_destroy_display(display);
        exit(-1);
    }

    timer = al_create_timer(1.0 / FPS);
    if (!timer) {
        al_show_native_message_box(NULL, "Error", "Timer Error",
            "Failed to create timer!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        al_destroy_event_queue(event_queue);
        al_destroy_display(display);
        exit(-1);
    }

    font = al_load_ttf_font(FONT_FILE, 24, 0); // 加載字體
    if (!font) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to load font: %s\nMake sure it's in the same directory as the executable.", FONT_FILE);
        al_show_native_message_box(display, "Error", "Font Error", error_msg, NULL, ALLEGRO_MESSAGEBOX_ERROR);
        al_destroy_timer(timer);
        al_destroy_event_queue(event_queue);
        al_destroy_display(display);
        exit(-1);
    }

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_mouse_event_source());
    al_register_event_source(event_queue, al_get_keyboard_event_source());

    // 可選：隱藏系統鼠標指針，如果你想自己繪製一個
    // al_hide_mouse_cursor(display);
}

// --- 釋放 Allegro 資源 ---
void shutdown_allegro() {
    if (font) al_destroy_font(font);
    if (timer) al_destroy_timer(timer);
    if (event_queue) al_destroy_event_queue(event_queue);
    if (display) al_destroy_display(display);
    al_shutdown_primitives_addon();
    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    // al_uninstall_mouse(); // Allegro 會自動處理
    // al_uninstall_keyboard(); // Allegro 會自動處理
    // al_uninstall_system(); // Allegro 會自動處理
}

// --- 產生新的目標位置 ---
void spawn_new_target() {
    // 確保目標完全在螢幕內
    target_x = rand() % (SCREEN_W - 2 * TARGET_RADIUS) + TARGET_RADIUS;
    target_y = rand() % (SCREEN_H - 2 * TARGET_RADIUS) + TARGET_RADIUS;
}

// --- 檢查點擊是否在目標上 ---
bool is_click_on_target(int click_x, int click_y) {
    // 計算點擊位置與目標中心的距離的平方
    // (x2-x1)^2 + (y2-y1)^2
    int dx = click_x - target_x;
    int dy = click_y - target_y;
    // 如果距離的平方小於半徑的平方，則點擊在圓內
    return (dx * dx + dy * dy) < (TARGET_RADIUS * TARGET_RADIUS);
}