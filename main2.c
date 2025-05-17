#define _CRT_SECURE_NO_WARNINGS // 如果您仍想用 sprintf 或其他被 MSVC 認為不安全的函式
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>

// --- 常數定義 ---
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FPS 60.0

// --- 新增：定義遊戲物件的目標繪製尺寸 ---
#define PLAYER_TARGET_WIDTH 64
#define PLAYER_TARGET_HEIGHT 64
#define ENEMY_TARGET_WIDTH 32
#define ENEMY_TARGET_HEIGHT 32
#define STAR_TARGET_WIDTH 32
#define STAR_TARGET_HEIGHT 32

// --- 資源檔案路徑 ---
// 建議將圖片檔案放在執行檔旁邊，並使用相對路徑
#define PLAYER_IMG_PATH "player.png"
#define STAR_IMG_PATH "star.jfif" // 例如: "star.jfif" 或 "images/star.jfif"
#define ENEMY_IMG_PATH "enemy.png" // 例如: "enemy.png" 或 "images/enemy.png"
#define BACKGROUND_IMG_PATH "background.png"
#define FONT_PATH "arial.ttf" // 請確保此字型檔案存在
#define BGM_PATH "bgm.mp3"
#define COLLECT_SFX_PATH "collect.mp3"
#define HIT_SFX_PATH "hit.mp3"

// --- 全域變數 ---
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;

ALLEGRO_BITMAP* player_image = NULL;
ALLEGRO_BITMAP* star_image = NULL;
ALLEGRO_BITMAP* enemy_image = NULL;
ALLEGRO_BITMAP* background_image = NULL;
ALLEGRO_FONT* game_font = NULL;
ALLEGRO_FONT* score_font = NULL;

ALLEGRO_SAMPLE* collect_sfx = NULL;
ALLEGRO_SAMPLE* hit_sfx = NULL;
ALLEGRO_AUDIO_STREAM* bgm = NULL;

bool game_running = true;
bool redraw = true;

float player_x, player_y;
float player_speed = 4.0;
int player_width = 0, player_height = 0; // 這些將被設為目標尺寸

typedef struct {
    float x, y;
    int width, height; // 這些將被設為目標尺寸
    bool active;
    ALLEGRO_BITMAP* image; // 指向原始圖片
} Collectible;
#define MAX_STARS 1
Collectible stars[MAX_STARS];

typedef struct {
    float x, y;
    int width, height; // 這些將被設為目標尺寸
    float speed;
    int direction;
    ALLEGRO_BITMAP* image; // 指向原始圖片
} Enemy;
#define MAX_ENEMIES 1
Enemy enemies[MAX_ENEMIES];

int score = 0;
bool game_over = false;

// --- 函數聲明 (部分) ---
bool engine_initialize();
void engine_load_resources();
void engine_handle_input(ALLEGRO_EVENT* ev);
void engine_update();
void engine_render();
void engine_cleanup();
void init_player();
void init_collectibles();
void init_enemies();
bool check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2);


// --- 遊戲引擎函式 ---
bool engine_initialize() {
    srand(time(NULL));

    if (!al_init()) {
        fprintf(stderr, "Failed to initialize Allegro!\n");
        return false;
    }
    if (!al_install_keyboard()) { fprintf(stderr, "Failed to install keyboard!\n"); return false; }
    timer = al_create_timer(1.0 / FPS);
    if (!timer) { fprintf(stderr, "Failed to create timer!\n"); return false; }
    display = al_create_display(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!display) { fprintf(stderr, "Failed to create display!\n"); al_destroy_timer(timer); return false; }
    al_set_window_title(display, "Enhanced Allegro Game (Scaled Sprites)");
    if (!al_init_primitives_addon()) { fprintf(stderr, "Failed to init primitives addon!\n"); return false; }
    if (!al_init_image_addon()) { fprintf(stderr, "Failed to init image addon!\n"); return false; }
    al_init_font_addon(); // 初始化字型插件
    if (!al_init_ttf_addon()) { fprintf(stderr, "Failed to init ttf addon!\n"); return false; } // 初始化 TTF 字型插件

    if (!al_install_audio()) {
        fprintf(stderr, "Failed to initialize audio!\n"); return false;
    }
    if (!al_init_acodec_addon()) {
        fprintf(stderr, "Failed to initialize audio codecs!\n"); return false;
    }
    if (!al_reserve_samples(2)) { // 為音效預留通道
        fprintf(stderr, "Failed to reserve samples!\n"); return false;
    }

    event_queue = al_create_event_queue();
    if (!event_queue) { fprintf(stderr, "Failed to create event queue!\n"); al_destroy_display(display); al_destroy_timer(timer); return false; }
    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_start_timer(timer);
    printf("Engine Initialized Successfully!\n");
    return true;
}

void init_player() {
    player_x = SCREEN_WIDTH / 2.0;
    // 設定玩家的寬度和高度為目標尺寸
    player_width = PLAYER_TARGET_WIDTH;
    player_height = PLAYER_TARGET_HEIGHT;
    // 根據目標高度調整初始 Y 位置
    player_y = SCREEN_HEIGHT - player_height - 10;
}

void init_collectibles() {
    if (!star_image) {
        printf("Star image not loaded, cannot initialize collectibles.\n");
        return;
    }
    for (int i = 0; i < MAX_STARS; ++i) {
        stars[i].image = star_image; // 指向全域的星星圖片
        // 設定星星的寬度和高度為目標尺寸
        stars[i].width = STAR_TARGET_WIDTH;
        stars[i].height = STAR_TARGET_HEIGHT;
        // 根據目標寬度隨機生成 X 位置
        stars[i].x = rand() % (SCREEN_WIDTH - stars[i].width);
        stars[i].y = rand() % (SCREEN_HEIGHT / 2); // 在螢幕上半部隨機生成 Y 位置
        stars[i].active = true;
    }
}

void init_enemies() {
    if (!enemy_image) {
        printf("Enemy image not loaded, cannot initialize enemies.\n");
        return;
    }
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        enemies[i].image = enemy_image; // 指向全域的敵人圖片
        // 設定敵人的寬度和高度為目標尺寸
        enemies[i].width = ENEMY_TARGET_WIDTH;
        enemies[i].height = ENEMY_TARGET_HEIGHT;
        // 根據目標寬度隨機生成 X 位置
        enemies[i].x = rand() % (SCREEN_WIDTH - enemies[i].width);
        // 調整 Y 軸生成範圍，確保敵人在指定範圍內且完整顯示
        enemies[i].y = 50 + rand() % (SCREEN_HEIGHT / 3 - enemies[i].height);
        if (enemies[i].y < 0) enemies[i].y = 50; // 避免 Y 座標為負
        enemies[i].speed = 2.0f + (float)(rand() % 100) / 100.0f; // 隨機速度
        enemies[i].direction = (rand() % 2 == 0) ? 1 : -1; // 隨機方向
    }
}

void engine_load_resources() {
    player_image = al_load_bitmap(PLAYER_IMG_PATH);
    if (!player_image) fprintf(stderr, "Failed to load %s!\n", PLAYER_IMG_PATH);

    star_image = al_load_bitmap(STAR_IMG_PATH);
    if (!star_image) fprintf(stderr, "Failed to load %s!\n", STAR_IMG_PATH);

    enemy_image = al_load_bitmap(ENEMY_IMG_PATH);
    if (!enemy_image) fprintf(stderr, "Failed to load %s!\n", ENEMY_IMG_PATH);

    background_image = al_load_bitmap(BACKGROUND_IMG_PATH);
    if (!background_image) printf("Optional: Failed to load %s.\n", BACKGROUND_IMG_PATH);

    game_font = al_load_ttf_font(FONT_PATH, 36, 0);
    if (!game_font) fprintf(stderr, "Could not load font: %s (game over)\n", FONT_PATH);
    score_font = al_load_ttf_font(FONT_PATH, 24, 0);
    if (!score_font) fprintf(stderr, "Could not load font: %s (score)\n", FONT_PATH);

    collect_sfx = al_load_sample(COLLECT_SFX_PATH);
    if (!collect_sfx) fprintf(stderr, "Failed to load %s!\n", COLLECT_SFX_PATH);
    hit_sfx = al_load_sample(HIT_SFX_PATH);
    if (!hit_sfx) fprintf(stderr, "Failed to load %s!\n", HIT_SFX_PATH);

    bgm = al_load_audio_stream(BGM_PATH, 4, 2048);
    if (!bgm) {
        fprintf(stderr, "Failed to load %s!\n", BGM_PATH);
    }
    else {
        al_set_audio_stream_playmode(bgm, ALLEGRO_PLAYMODE_LOOP);
        al_set_audio_stream_gain(bgm, 0.5); // 設定 BGM 音量
        if (al_attach_audio_stream_to_mixer(bgm, al_get_default_mixer())) {
            al_set_audio_stream_playing(bgm, true);
            printf("BGM started (direct stream play).\n");
        }
        else {
            fprintf(stderr, "Failed to attach BGM stream to mixer.\n");
            al_destroy_audio_stream(bgm);
            bgm = NULL;
        }
    }

    // 初始化遊戲物件 (這時會使用目標尺寸)
    init_player();
    init_collectibles();
    init_enemies();
    printf("Resources Loaded.\n");
}

bool check_collision(float x1, float y1, int w1, int h1, float x2, float y2, int w2, int h2) {
    // 碰撞檢測邏輯保持不變，因為 w1, h1, w2, h2 將傳入物件的目標尺寸
    return !(x1 + w1 < x2 || x1 > x2 + w2 || y1 + h1 < y2 || y1 > y2 + h2);
}

void engine_handle_input(ALLEGRO_EVENT* ev) {
    if (ev->type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
        game_running = false;
        return;
    }

    // 遊戲結束時的輸入處理 (R鍵重新開始, ESC離開)
    if (game_over) {
        if (ev->type == ALLEGRO_EVENT_KEY_DOWN) { // 只處理按下事件
            if (ev->keyboard.keycode == ALLEGRO_KEY_R) {
                game_over = false;
                score = 0;
                init_player();
                init_collectibles();
                init_enemies();
                if (bgm && !al_get_audio_stream_playing(bgm)) {
                    al_set_audio_stream_playing(bgm, true);
                }
            }
            else if (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                game_running = false;
            }
        }
        return; // 如果遊戲結束，不處理後續的遊戲中按鍵
    }

    // 遊戲進行中的按鍵處理 (主要處理非移動的、一次性的按鍵)
    if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
        switch (ev->keyboard.keycode) {
            // 移動鍵的處理移到 engine_update
        case ALLEGRO_KEY_ESCAPE:
            game_running = false;
            break;
            // 如果有其他一次性按鍵 (如跳躍、射擊)，可以在這裡處理 KEY_DOWN
        }
    }
    // KEY_UP 事件通常用於釋放持續性動作，但由於我們用 al_get_keyboard_state()，這裡也不太需要了
}

void engine_update() {
    if (game_over) return;

    // --- 新增：使用 al_get_keyboard_state() 處理玩家持續移動 ---
    ALLEGRO_KEYBOARD_STATE key_state;
    al_get_keyboard_state(&key_state); // 獲取當前鍵盤狀態

    if (al_key_down(&key_state, ALLEGRO_KEY_UP) || al_key_down(&key_state, ALLEGRO_KEY_W)) {
        player_y -= player_speed;
    }
    if (al_key_down(&key_state, ALLEGRO_KEY_DOWN) || al_key_down(&key_state, ALLEGRO_KEY_S)) {
        player_y += player_speed;
    }
    if (al_key_down(&key_state, ALLEGRO_KEY_LEFT) || al_key_down(&key_state, ALLEGRO_KEY_A)) {
        player_x -= player_speed;
    }
    if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT) || al_key_down(&key_state, ALLEGRO_KEY_D)) {
        player_x += player_speed;
    }
    // --- 結束新增 ---


    // 邊界檢測使用 player_width 和 player_height (已設為目標尺寸)
    if (player_x < 0) player_x = 0;
    if (player_x > SCREEN_WIDTH - player_width) player_x = SCREEN_WIDTH - player_width;
    if (player_y < 0) player_y = 0;
    if (player_y > SCREEN_HEIGHT - player_height) player_y = SCREEN_HEIGHT - player_height;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        enemies[i].x += enemies[i].speed * enemies[i].direction;
        // 邊界檢測使用 enemies[i].width (已設為目標尺寸)
        if (enemies[i].x <= 0 || enemies[i].x + enemies[i].width >= SCREEN_WIDTH) {
            enemies[i].direction *= -1; // 改變方向
            // 確保敵人不會卡在邊界外
            if (enemies[i].x <= 0) enemies[i].x = 0;
            if (enemies[i].x + enemies[i].width >= SCREEN_WIDTH) enemies[i].x = SCREEN_WIDTH - enemies[i].width;
        }
    }

    for (int i = 0; i < MAX_STARS; ++i) {
        if (stars[i].active &&
            check_collision(player_x, player_y, player_width, player_height,
                stars[i].x, stars[i].y, stars[i].width, stars[i].height)) {
            stars[i].active = false;
            score += 10;
            if (collect_sfx) al_play_sample(collect_sfx, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
            printf("Star collected! Score: %d\n", score);

            bool all_collected = true;
            for (int j = 0; j < MAX_STARS; ++j) {
                if (stars[j].active) {
                    all_collected = false;
                    break;
                }
            }
            if (all_collected) {
                printf("All stars collected! Regenerating...\n");
                init_collectibles(); // 重新生成星星
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (check_collision(player_x, player_y, player_width, player_height,
            enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height)) {
            if (hit_sfx) al_play_sample(hit_sfx, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
            printf("Hit by enemy! Game Over.\n");
            game_over = true;
            if (bgm) {
                al_set_audio_stream_playing(bgm, false); // 遊戲結束時停止 BGM
            }
        }
    }
}

void engine_render() {
    if (background_image) {
        // 繪製背景，使其填滿螢幕 (如果背景圖較小，可以考慮縮放或重複繪製)
        al_draw_scaled_bitmap(background_image,
            0, 0, al_get_bitmap_width(background_image), al_get_bitmap_height(background_image),
            0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    }
    else {
        al_clear_to_color(al_map_rgb(20, 20, 80));
    }


    // 繪製星星 (縮放)
    if (star_image) { // 檢查全域 star_image 是否載入
        for (int i = 0; i < MAX_STARS; ++i) {
            if (stars[i].active && stars[i].image) { // 也檢查 stars[i].image 是否有效
                al_draw_scaled_bitmap(stars[i].image,
                    0, 0, al_get_bitmap_width(stars[i].image), al_get_bitmap_height(stars[i].image), // 來源圖片的完整區域
                    stars[i].x, stars[i].y, stars[i].width, stars[i].height, // 目標位置和目標尺寸
                    0);
            }
        }
    }
    // 繪製敵人 (縮放)
    if (enemy_image) { // 檢查全域 enemy_image 是否載入
        for (int i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].image) { // 也檢查 enemies[i].image 是否有效
                al_draw_scaled_bitmap(enemies[i].image,
                    0, 0, al_get_bitmap_width(enemies[i].image), al_get_bitmap_height(enemies[i].image), // 來源圖片的完整區域
                    enemies[i].x, enemies[i].y, enemies[i].width, enemies[i].height, // 目標位置和目標尺寸
                    0);
            }
        }
    }
    // 繪製玩家 (縮放)
    if (player_image) {
        al_draw_scaled_bitmap(player_image,
            0, 0, al_get_bitmap_width(player_image), al_get_bitmap_height(player_image), // 來源圖片的完整區域
            player_x, player_y, player_width, player_height, // 目標位置和目標尺寸
            0);
    }
    else { // 如果圖片載入失敗，繪製一個使用目標尺寸的矩形作為替代
        al_draw_filled_rectangle(player_x, player_y, player_x + player_width, player_y + player_height, al_map_rgb(0, 255, 0));
    }

    if (score_font) {
        char score_text[50];
        snprintf(score_text, sizeof(score_text), "Score: %d", score);
        al_draw_text(score_font, al_map_rgb(255, 255, 255), 10, 10, ALLEGRO_ALIGN_LEFT, score_text);
    }

    if (game_over && game_font) {
        al_draw_text(game_font, al_map_rgb(255, 0, 0), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 50, ALLEGRO_ALIGN_CENTRE, "GAME OVER");
        if (score_font) { // 使用較小的字型顯示提示訊息
            al_draw_text(score_font, al_map_rgb(255, 255, 255), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, ALLEGRO_ALIGN_CENTRE, "Press 'R' to Restart or 'ESC' to Exit");
        }
    }
    al_flip_display();
}

void engine_cleanup() {
    printf("Engine Cleaning Up...\n");
    if (player_image) al_destroy_bitmap(player_image);
    if (star_image) al_destroy_bitmap(star_image);
    if (enemy_image) al_destroy_bitmap(enemy_image);
    if (background_image) al_destroy_bitmap(background_image);
    if (game_font) al_destroy_font(game_font);
    if (score_font) al_destroy_font(score_font);
    if (collect_sfx) al_destroy_sample(collect_sfx);
    if (hit_sfx) al_destroy_sample(hit_sfx);

    if (bgm) {
        al_destroy_audio_stream(bgm);
    }

    if (timer) al_destroy_timer(timer);
    if (display) al_destroy_display(display);
    if (event_queue) al_destroy_event_queue(event_queue);

    // 確保按照正確順序卸載 Allegro 插件
    // 這些卸載函數會檢查相應的插件是否已初始化
    al_shutdown_ttf_addon();
    al_shutdown_font_addon();
    al_shutdown_image_addon();
    al_shutdown_primitives_addon();
    al_uninstall_audio(); // 卸載音訊系統
    al_uninstall_keyboard(); // 卸載鍵盤系統
    // al_uninstall_system(); // 通常 al_init() 的逆操作，在程式結束時自動處理或由 OS 處理。
                            // 在此處顯式調用可能導致後續的 printf 等無法正常工作。
                            // 一般情況下不需要手動調用。

    printf("Engine Cleanup Complete.\n");
}
/*
int main(int argc, char** argv) {
    if (!engine_initialize()) {
        fprintf(stderr, "Game engine failed to initialize.\n");
        return -1;
    }
    engine_load_resources(); // 資源載入後，物件的尺寸已根據目標尺寸設定

    while (game_running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev); // 等待事件

        if (ev.type == ALLEGRO_EVENT_TIMER) { // 計時器事件
            // engine_update 內部會檢查 game_over
            engine_update(); // 更新遊戲邏輯 (現在包含鍵盤狀態檢測和移動)
            redraw = true; // 設定重繪標記
        }
        // 其他事件，如鍵盤(用於非移動操作)、顯示關閉等
        engine_handle_input(&ev);


        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            engine_render(); // 渲染畫面
        }
    }

    engine_cleanup(); // 清理資源
    return 0;
}
*/