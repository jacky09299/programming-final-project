#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

#define SCREEN_W 800
#define SCREEN_H 600
#define FPS 60.0

#define PLAYER_RADIUS 25.0f
#define ENEMY_RADIUS 20.0f
// --- Player Movement ---
#define PLAYER_MAX_SPEED 4.0f     // 產笷程硉
#define PLAYER_ACCELERATION 0.2f // 產–碫硉
// --- Enemy Movement ---
#define ENEMY_INIT_MAX_SPEED 1.5f // 寄﹍繦诀硉程
#define NUM_ENEMIES 5

// --- Softness Parameters ---
#define DEFORMATION_DURATION 0.3f
#define DEFORMATION_FACTOR_PRIMARY 0.5f
#define DEFORMATION_FACTOR_SECONDARY 1.4f
#define COEFFICIENT_OF_RESTITUTION 0.75f
#define WALL_COEFFICIENT_OF_RESTITUTION 0.6f

#define BALL_SPRITE_RADIUS 32.0f

#ifndef ALLEGRO_PI
#define ALLEGRO_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y;
    float vx, vy;
    float base_radius;
    float current_radius_primary;
    float current_radius_secondary;
    ALLEGRO_COLOR color;
    bool is_deforming;
    double deformation_start_time;
    float deformation_angle;
} SoftBall;

ALLEGRO_BITMAP* ball_sprite = NULL;

float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

void init_ball(SoftBall* ball, float x, float y, float radius, ALLEGRO_COLOR color, float initial_vx, float initial_vy) {
    ball->x = x;
    ball->y = y;
    ball->vx = initial_vx;
    ball->vy = initial_vy;
    ball->base_radius = radius;
    ball->current_radius_primary = radius;
    ball->current_radius_secondary = radius;
    ball->color = color;
    ball->is_deforming = false;
    ball->deformation_start_time = 0;
    ball->deformation_angle = 0;
}

// Overload for enemies with random speed
void init_enemy_ball(SoftBall* ball, float x, float y, float radius, ALLEGRO_COLOR color, float speed_range) {
    float vx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * speed_range;
    float vy = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * speed_range;
    init_ball(ball, x, y, radius, color, vx, vy);
}


void trigger_deformation(SoftBall* ball, double current_time, float collision_nx, float collision_ny) {
    ball->is_deforming = true;
    ball->deformation_start_time = current_time;
    ball->deformation_angle = atan2(collision_ny, collision_nx);
}

void update_deformation(SoftBall* ball, double current_time) {
    if (ball->is_deforming) {
        double elapsed = current_time - ball->deformation_start_time;
        if (elapsed >= DEFORMATION_DURATION) {
            ball->is_deforming = false;
            ball->current_radius_primary = ball->base_radius;
            ball->current_radius_secondary = ball->base_radius;
        }
        else {
            float progress_rad = (elapsed / DEFORMATION_DURATION) * ALLEGRO_PI;
            float deformation_phase = sin(progress_rad);
            ball->current_radius_primary = ball->base_radius * (1.0f - deformation_phase * (1.0f - DEFORMATION_FACTOR_PRIMARY));
            ball->current_radius_secondary = ball->base_radius * (1.0f + deformation_phase * (DEFORMATION_FACTOR_SECONDARY - 1.0f));
        }
    }
}

void update_ball_position(SoftBall* ball, double current_time) {
    ball->x += ball->vx;
    ball->y += ball->vy;

    bool collided_wall = false;
    float wall_collision_nx = 0, wall_collision_ny = 0;

    if (ball->x - ball->base_radius < 0) {
        ball->x = ball->base_radius;
        ball->vx *= -WALL_COEFFICIENT_OF_RESTITUTION;
        collided_wall = true; wall_collision_nx = 1.0f; wall_collision_ny = 0.0f;
    }
    else if (ball->x + ball->base_radius > SCREEN_W) {
        ball->x = SCREEN_W - ball->base_radius;
        ball->vx *= -WALL_COEFFICIENT_OF_RESTITUTION;
        collided_wall = true; wall_collision_nx = -1.0f; wall_collision_ny = 0.0f;
    }
    if (ball->y - ball->base_radius < 0) {
        ball->y = ball->base_radius;
        ball->vy *= -WALL_COEFFICIENT_OF_RESTITUTION;
        if (collided_wall) {
            wall_collision_nx = (wall_collision_nx + 0.0f); // Don't divide by 2 here, normalize later
            wall_collision_ny = (wall_collision_ny + 1.0f);
        }
        else {
            collided_wall = true; wall_collision_nx = 0.0f; wall_collision_ny = 1.0f;
        }
    }
    else if (ball->y + ball->base_radius > SCREEN_H) {
        ball->y = SCREEN_H - ball->base_radius;
        ball->vy *= -WALL_COEFFICIENT_OF_RESTITUTION;
        if (collided_wall) {
            wall_collision_nx = (wall_collision_nx + 0.0f);
            wall_collision_ny = (wall_collision_ny - 1.0f);
        }
        else {
            collided_wall = true; wall_collision_nx = 0.0f; wall_collision_ny = -1.0f;
        }
    }

    if (collided_wall) {
        float len_sq = wall_collision_nx * wall_collision_nx + wall_collision_ny * wall_collision_ny;
        if (len_sq > 0.001f) {
            float len = sqrt(len_sq);
            wall_collision_nx /= len;
            wall_collision_ny /= len;
            trigger_deformation(ball, current_time, wall_collision_nx, wall_collision_ny);
        }
    }
}

void handle_ball_collision(SoftBall* b1, SoftBall* b2, double current_time) {
    float dx = b2->x - b1->x;
    float dy = b2->y - b1->y;
    float distance_squared = dx * dx + dy * dy;
    float sum_radii = b1->base_radius + b2->base_radius;

    if (distance_squared < sum_radii * sum_radii && distance_squared > 0.001f) {
        float distance = sqrt(distance_squared);
        float overlap = sum_radii - distance;
        float nx = dx / distance;
        float ny = dy / distance;
        float separation_amount = overlap * 0.5f;
        b1->x -= nx * separation_amount;
        b1->y -= ny * separation_amount;
        b2->x += nx * separation_amount;
        b2->y += ny * separation_amount;

        float v1_normal_scalar = b1->vx * nx + b1->vy * ny;
        float v1_tx = b1->vx - v1_normal_scalar * nx;
        float v1_ty = b1->vy - v1_normal_scalar * ny;
        float v2_normal_scalar = b2->vx * nx + b2->vy * ny;
        float v2_tx = b2->vx - v2_normal_scalar * nx;
        float v2_ty = b2->vy - v2_normal_scalar * ny;

        if (v1_normal_scalar - v2_normal_scalar > 0) {
            float e = COEFFICIENT_OF_RESTITUTION;
            float new_v1_normal_scalar = (v1_normal_scalar * (1.0f - e) + v2_normal_scalar * (1.0f + e)) / 2.0f;
            float new_v2_normal_scalar = (v1_normal_scalar * (1.0f + e) + v2_normal_scalar * (1.0f - e)) / 2.0f;
            b1->vx = new_v1_normal_scalar * nx + v1_tx;
            b1->vy = new_v1_normal_scalar * ny + v1_ty;
            b2->vx = new_v2_normal_scalar * nx + v2_tx;
            b2->vy = new_v2_normal_scalar * ny + v2_ty;
            trigger_deformation(b1, current_time, -nx, -ny);
            trigger_deformation(b2, current_time, nx, ny);
        }
    }
}

int main() {
    if (!al_init()) { fprintf(stderr, "Failed to initialize Allegro!\n"); return -1; }
    if (!al_install_keyboard()) { fprintf(stderr, "Failed to install keyboard!\n"); return -1; }
    if (!al_init_primitives_addon()) { fprintf(stderr, "Failed to initialize primitives addon!\n"); return -1; }
    al_init_font_addon();
    al_init_ttf_addon();
    ALLEGRO_FONT* font = al_load_ttf_font("arial.ttf", 18, 0);
    if (!font && SCREEN_W > 0) {
        fprintf(stderr, "Failed to load 'arial.ttf'. Text display might be affected.\n");
    }

    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    ALLEGRO_DISPLAY* display = al_create_display(SCREEN_W, SCREEN_H);
    ALLEGRO_EVENT_QUEUE* event_queue = al_create_event_queue();

    if (!timer || !display || !event_queue) {
        fprintf(stderr, "Failed to create Allegro resources!\n");
        if (timer) al_destroy_timer(timer);
        if (display) al_destroy_display(display);
        if (event_queue) al_destroy_event_queue(event_queue);
        return -1;
    }

    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_display_event_source(display));

    srand(time(NULL));

    ball_sprite = al_create_bitmap(BALL_SPRITE_RADIUS * 2, BALL_SPRITE_RADIUS * 2);
    if (!ball_sprite) { fprintf(stderr, "Failed to create ball sprite bitmap!\n"); return -1; }
    ALLEGRO_STATE old_state;
    al_store_state(&old_state, ALLEGRO_STATE_TARGET_BITMAP);
    al_set_target_bitmap(ball_sprite);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0));
    al_draw_filled_circle(BALL_SPRITE_RADIUS, BALL_SPRITE_RADIUS, BALL_SPRITE_RADIUS, al_map_rgb(255, 255, 255));
    al_restore_state(&old_state);

    SoftBall player;
    // Player starts stationary
    init_ball(&player, SCREEN_W / 2.0f, SCREEN_H / 2.0f, PLAYER_RADIUS, al_map_rgb(0, 255, 0), 0.0f, 0.0f);

    SoftBall enemies[NUM_ENEMIES];
    for (int i = 0; i < NUM_ENEMIES; ++i) {
        init_enemy_ball(&enemies[i], // Use specific enemy init
            (float)rand() / RAND_MAX * (SCREEN_W - ENEMY_RADIUS * 2) + ENEMY_RADIUS,
            (float)rand() / RAND_MAX * (SCREEN_H - ENEMY_RADIUS * 2) + ENEMY_RADIUS,
            ENEMY_RADIUS,
            al_map_rgb(rand() % 156 + 100, rand() % 156 + 100, rand() % 156 + 100),
            ENEMY_INIT_MAX_SPEED);
    }

    bool key_pressed[ALLEGRO_KEY_MAX] = { false };
    bool running = true;
    bool redraw = true;

    al_start_timer(timer);
    double current_time = al_get_time();

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);

        if (ev.type == ALLEGRO_EVENT_TIMER) {
            current_time = al_get_time();

            // --- 穝產硉 (硉㎝程硉) ---
            if (key_pressed[ALLEGRO_KEY_UP] || key_pressed[ALLEGRO_KEY_W]) player.vy -= PLAYER_ACCELERATION;
            if (key_pressed[ALLEGRO_KEY_DOWN] || key_pressed[ALLEGRO_KEY_S]) player.vy += PLAYER_ACCELERATION;
            if (key_pressed[ALLEGRO_KEY_LEFT] || key_pressed[ALLEGRO_KEY_A]) player.vx -= PLAYER_ACCELERATION;
            if (key_pressed[ALLEGRO_KEY_RIGHT] || key_pressed[ALLEGRO_KEY_D]) player.vx += PLAYER_ACCELERATION;

            // 產程硉
            float player_speed_sq = player.vx * player.vx + player.vy * player.vy;
            if (player_speed_sq > PLAYER_MAX_SPEED * PLAYER_MAX_SPEED) {
                float player_speed_mag = sqrt(player_speed_sq);
                player.vx = (player.vx / player_speed_mag) * PLAYER_MAX_SPEED;
                player.vy = (player.vy / player_speed_mag) * PLAYER_MAX_SPEED;
            }
            // --- 產硉穝挡 ---

            update_ball_position(&player, current_time);
            update_deformation(&player, current_time);

            for (int i = 0; i < NUM_ENEMIES; ++i) {
                update_ball_position(&enemies[i], current_time);
                update_deformation(&enemies[i], current_time);
            }

            for (int i = 0; i < NUM_ENEMIES; ++i) {
                handle_ball_collision(&player, &enemies[i], current_time);
            }
            for (int i = 0; i < NUM_ENEMIES; ++i) {
                for (int j = i + 1; j < NUM_ENEMIES; ++j) {
                    handle_ball_collision(&enemies[i], &enemies[j], current_time);
                }
            }
            redraw = true;
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            key_pressed[ev.keyboard.keycode] = true;
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_UP) {
            key_pressed[ev.keyboard.keycode] = false;
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                running = false;
            }
        }

        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(30, 30, 30));

            float scale_x_player = player.current_radius_secondary / BALL_SPRITE_RADIUS;
            float scale_y_player = player.current_radius_primary / BALL_SPRITE_RADIUS;
            al_draw_tinted_scaled_rotated_bitmap(ball_sprite, player.color,
                BALL_SPRITE_RADIUS, BALL_SPRITE_RADIUS,
                player.x, player.y,
                scale_x_player, scale_y_player,
                player.deformation_angle - ALLEGRO_PI / 2.0f, 0);

            for (int i = 0; i < NUM_ENEMIES; ++i) {
                float scale_x_enemy = enemies[i].current_radius_secondary / BALL_SPRITE_RADIUS;
                float scale_y_enemy = enemies[i].current_radius_primary / BALL_SPRITE_RADIUS;
                al_draw_tinted_scaled_rotated_bitmap(ball_sprite, enemies[i].color,
                    BALL_SPRITE_RADIUS, BALL_SPRITE_RADIUS,
                    enemies[i].x, enemies[i].y,
                    scale_x_enemy, scale_y_enemy,
                    enemies[i].deformation_angle - ALLEGRO_PI / 2.0f, 0);
            }

            if (font) {
                al_draw_text(font, al_map_rgb(255, 255, 255), 10, 10, 0, "WASD/Arrows: Accelerate. ESC: Quit.");
            }
            al_flip_display();
        }
    }

    if (ball_sprite) al_destroy_bitmap(ball_sprite);
    if (font) al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_display(display);
    al_destroy_event_queue(event_queue);
    al_shutdown_primitives_addon();
    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    return 0;
}
