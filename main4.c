#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_native_dialog.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// --- 常數定義 ---
// 將 const int 改為 #define
#define SCREEN_W 800
#define SCREEN_H 600
const float FPS = 60.0; // float 常量通常沒問題，因為 60.0 本身是常量
const char* FONT_FILE = "arial.ttf"; // 字串字面量是常量

// --- 角色屬性 ---
// 現在 SCREEN_W 和 SCREEN_H 是透過 #define 定義的，所以下面的初始化是常量表達式
float player_x = SCREEN_W / 2.0f;
float player_y = SCREEN_H / 2.0f;
float player_speed = 2.0f;
int player_body_width = 20;
int player_body_height = 40;
int player_leg_width = 6;
int player_leg_length = 25;
int player_head_radius = 10;

// --- 目標位置 ---
float target_x = SCREEN_W / 2.0f;
float target_y = SCREEN_H / 2.0f;
bool is_moving = false;

// --- 動畫相關 ---
int animation_frame = 0;
int animation_timer = 0;
const int ANIMATION_DELAY = 15; // 15 是常量，所以這裡沒問題

// --- 全局 Allegro 變數 ---
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;
ALLEGRO_FONT* font = NULL; // 可選

// --- 函數原型 ---
void init_allegro();
void shutdown_allegro();
void draw_player();
void update_player_movement();

// --- 主函數 ---
int main(int argc, char** argv) {
    init_allegro();
    srand(time(NULL));

    bool running = true;
    bool redraw = true;

    al_start_timer(timer);

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);

        if (ev.type == ALLEGRO_EVENT_TIMER) {
            if (is_moving) {
                update_player_movement();
                animation_timer++;
                if (animation_timer >= ANIMATION_DELAY) {
                    animation_frame = 1 - animation_frame; // 切換 0 和 1
                    animation_timer = 0;
                }
            }
            else {
                animation_frame = 0; // 停止時恢復預設站姿 (或者保持最後一幀)
            }
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
        else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev.mouse.button == 1) { // 左鍵點擊
                target_x = ev.mouse.x;
                target_y = ev.mouse.y;
                is_moving = true;
            }
        }

        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(100, 149, 237)); // 天藍色背景

            draw_player();

            // (可選) 繪製目標點
            if (is_moving) {
                al_draw_filled_circle(target_x, target_y, 5, al_map_rgb(255, 255, 0));
            }

            al_flip_display();
        }
    }

    shutdown_allegro();
    return 0;
}

// --- 初始化 Allegro ---
void init_allegro() {
    if (!al_init()) {
        al_show_native_message_box(NULL, "Error", "Allegro Error", "Failed to initialize allegro!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    if (!al_init_primitives_addon()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error", "Failed to initialize primitives addon!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    if (!al_install_mouse()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error", "Failed to install mouse!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    if (!al_install_keyboard()) {
        al_show_native_message_box(NULL, "Error", "Allegro Addon Error", "Failed to install keyboard!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    // al_init_font_addon(); // 如果要用字體
    // if (!al_init_ttf_addon()) { /* ... */ }

    display = al_create_display(SCREEN_W, SCREEN_H); // SCREEN_W, SCREEN_H 已被 #define 替換
    if (!display) {
        al_show_native_message_box(NULL, "Error", "Display Error", "Failed to create display!", NULL, ALLEGRO_MESSAGEBOX_ERROR);
        exit(-1);
    }
    al_set_window_title(display, "Walk To Click");

    event_queue = al_create_event_queue();
    if (!event_queue) { al_destroy_display(display); exit(-1); }
    timer = al_create_timer(1.0 / FPS); // FPS 是 const float，但 1.0 / 60.0 是常量表達式
    if (!timer) { al_destroy_event_queue(event_queue); al_destroy_display(display); exit(-1); }

    // font = al_load_ttf_font(FONT_FILE, 18, 0); // 如果要用字體
    // if (!font) { /* ... */ }

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_mouse_event_source());
    al_register_event_source(event_queue, al_get_keyboard_event_source());
}

// --- 釋放 Allegro 資源 ---
void shutdown_allegro() {
    // if (font) al_destroy_font(font);
    if (timer) al_destroy_timer(timer);
    if (event_queue) al_destroy_event_queue(event_queue);
    if (display) al_destroy_display(display);
    al_shutdown_primitives_addon();
    // al_shutdown_font_addon();
    // al_shutdown_ttf_addon();
}

// --- 繪製角色 ---
void draw_player() {
    ALLEGRO_COLOR body_color = al_map_rgb(50, 50, 200);
    ALLEGRO_COLOR leg_color = al_map_rgb(30, 30, 150);
    ALLEGRO_COLOR head_color = al_map_rgb(255, 224, 189);

    float body_x1 = player_x - player_body_width / 2.0f;
    float body_y1 = player_y - player_body_height;
    float body_x2 = player_x + player_body_width / 2.0f;
    float body_y2 = player_y;
    al_draw_filled_rectangle(body_x1, body_y1, body_x2, body_y2, body_color);

    float head_center_x = player_x;
    float head_center_y = body_y1 - player_head_radius;
    al_draw_filled_circle(head_center_x, head_center_y, player_head_radius, head_color);

    float leg_top_y = player_y;
    float leg_bottom_y = player_y + player_leg_length;
    float leg_offset_factor = 1.0f; // 調整腿部張開的幅度

    float left_leg_x_center_base = player_x - player_leg_width * 0.75f; // 左腿的基礎 X 位置 (稍微偏左)
    float right_leg_x_center_base = player_x + player_leg_width * 0.75f; // 右腿的基礎 X 位置 (稍微偏右)

    float leg_swing_offset = player_leg_width * leg_offset_factor; // 腿前後擺動的幅度

    float left_leg_x_current = left_leg_x_center_base;
    float right_leg_x_current = right_leg_x_center_base;

    if (is_moving) {
        if (animation_frame == 0) { // 左腿向前，右腿向後 (相對中心)
            left_leg_x_current -= leg_swing_offset / 2;
            right_leg_x_current += leg_swing_offset / 2;
        }
        else { // 左腿向後，右腿向前
            left_leg_x_current += leg_swing_offset / 2;
            right_leg_x_current -= leg_swing_offset / 2;
        }
    }
    // 即使不移動，也讓腿稍微分開
    else {
        // 靜止時腿可以稍微分開或併攏，這裡保持基礎位置
    }


    // 左腿
    al_draw_filled_rectangle(left_leg_x_current - player_leg_width / 2.0f, leg_top_y,
        left_leg_x_current + player_leg_width / 2.0f, leg_bottom_y, leg_color);

    // 右腿
    al_draw_filled_rectangle(right_leg_x_current - player_leg_width / 2.0f, leg_top_y,
        right_leg_x_current + player_leg_width / 2.0f, leg_bottom_y, leg_color);
}


// --- 更新角色移動 ---
void update_player_movement() {
    float dx = target_x - player_x;
    float dy = target_y - player_y;
    float distance = sqrt(dx * dx + dy * dy);

    if (distance < player_speed) {
        player_x = target_x;
        player_y = target_y;
        is_moving = false;
    }
    else {
        float move_x = (dx / distance) * player_speed;
        float move_y = (dy / distance) * player_speed;

        player_x += move_x;
        player_y += move_y;
    }
}