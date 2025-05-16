#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCREEN_W 800
#define SCREEN_H 600
#define FPS 60.0 // Still useful for redrawing smoothly if needed, though rotation is mouse-driven
#define TETRA_SIZE 100
#define MOUSE_SENSITIVITY 0.005f // Adjust for how much rotation per mouse movement

typedef struct {
    float x, y, z;
} Point3D;

typedef struct {
    int v_idx[3];
    float avg_z;
    Point3D normal;
    ALLEGRO_COLOR current_color;
} Face;

Point3D original_vertices[4];
Point3D transformed_vertices[4];
Face faces[4];

ALLEGRO_COLOR tetra_base_color;
Point3D light_direction;
float ambient_light_intensity = 0.2f;
float diffuse_light_intensity = 0.8f;

float angle_x = 0.0f;
float angle_y = 0.0f;
// float angle_z = 0.0f; // Can add Z rotation with another mouse button or modifier key

// Mouse state
bool is_dragging = false;
int last_mouse_x = 0;
int last_mouse_y = 0;


Point3D vec_subtract(Point3D a, Point3D b) {
    return (Point3D) { a.x - b.x, a.y - b.y, a.z - b.z };
}

Point3D vec_cross_product(Point3D a, Point3D b) {
    return (Point3D) {
        a.y* b.z - a.z * b.y,
            a.z* b.x - a.x * b.z,
            a.x* b.y - a.y * b.x
    };
}

float vec_dot_product(Point3D a, Point3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec_magnitude(Point3D v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Point3D vec_normalize(Point3D v) {
    float mag = vec_magnitude(v);
    if (mag == 0.0f) return (Point3D) { 0, 0, 0 };
    return (Point3D) { v.x / mag, v.y / mag, v.z / mag };
}

int init_allegro() {
    if (!al_init()) { fprintf(stderr, "Failed to initialize Allegro!\n"); return -1; }
    if (!al_install_keyboard()) { fprintf(stderr, "Failed to initialize the keyboard!\n"); return -1; }
    if (!al_install_mouse()) { fprintf(stderr, "Failed to initialize the mouse!\n"); return -1; } // Initialize mouse
    if (!al_init_primitives_addon()) { fprintf(stderr, "Failed to initialize primitives addon!\n"); return -1; }
    al_init_font_addon();
    al_init_ttf_addon();
    return 0;
}

void define_tetrahedron() {
    float s = TETRA_SIZE;
    original_vertices[0] = (Point3D){ s,  s,  s };
    original_vertices[1] = (Point3D){ s, -s, -s };
    original_vertices[2] = (Point3D){ -s,  s, -s };
    original_vertices[3] = (Point3D){ -s, -s,  s };

    // CCW from outside for correct normals
    faces[0] = (Face){ {0, 1, 2} };
    faces[1] = (Face){ {0, 2, 3} };
    faces[2] = (Face){ {0, 3, 1} };
    faces[3] = (Face){ {1, 3, 2} }; // Bottom face (1,3,2) if looking from outside (below) is CCW

    tetra_base_color = al_map_rgb(100, 100, 200);
    light_direction = vec_normalize((Point3D) { 0.5f, 0.5f, -1.0f });
}

Point3D rotate_x(Point3D p, float angle) {
    Point3D rp;
    rp.x = p.x;
    rp.y = p.y * cos(angle) - p.z * sin(angle);
    rp.z = p.y * sin(angle) + p.z * cos(angle);
    return rp;
}

Point3D rotate_y(Point3D p, float angle) {
    Point3D rp;
    rp.x = p.x * cos(angle) + p.z * sin(angle);
    rp.y = p.y;
    rp.z = -p.x * sin(angle) + p.z * cos(angle);
    return rp;
}


int compare_faces(const void* a, const void* b) {
    Face* faceA = (Face*)a;
    Face* faceB = (Face*)b;
    if (faceA->avg_z < faceB->avg_z) return 1;
    if (faceA->avg_z > faceB->avg_z) return -1;
    return 0;
}

int main() {
    ALLEGRO_DISPLAY* display = NULL;
    ALLEGRO_EVENT_QUEUE* event_queue = NULL;
    ALLEGRO_TIMER* timer = NULL; // Still useful for consistent redraw rate
    ALLEGRO_FONT* font = NULL;

    if (init_allegro() != 0) { return -1; }

    display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) { /* ... */ return -1; }
    timer = al_create_timer(1.0 / FPS); // Redraw at FPS rate
    if (!timer) { /* ... */ return -1; }
    event_queue = al_create_event_queue();
    if (!event_queue) { /* ... */ return -1; }

    font = al_load_ttf_font("arial.ttf", 18, 0);
    if (!font) {
        font = al_create_builtin_font();
        fprintf(stderr, "Failed to load 'arial.ttf', using built-in font.\n");
    }

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer)); // Keep timer for redraw
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_mouse_event_source()); // Register mouse events

    define_tetrahedron();

    // Initial draw
    bool redraw = true; // Force initial draw
    al_start_timer(timer);

    bool running = true;

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);

        if (ev.type == ALLEGRO_EVENT_TIMER) {
            // Timer event is now primarily for triggering redraws if nothing else caused it.
            // The actual angle changes happen on mouse events.
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
            if (ev.mouse.button == 1) { // Left mouse button
                is_dragging = true;
                last_mouse_x = ev.mouse.x;
                last_mouse_y = ev.mouse.y;
            }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
            if (ev.mouse.button == 1) { // Left mouse button
                is_dragging = false;
            }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES || ev.type == ALLEGRO_EVENT_MOUSE_WARPED) {
            if (is_dragging) {
                int mouse_dx = ev.mouse.x - last_mouse_x;
                int mouse_dy = ev.mouse.y - last_mouse_y;

                angle_y -= mouse_dx * MOUSE_SENSITIVITY; // Horizontal mouse movement -> Y-axis rotation
                angle_x -= mouse_dy * MOUSE_SENSITIVITY; // Vertical mouse movement -> X-axis rotation

                // Optional: Clamp angles or let them wrap around (cos/sin handle it)
                // if (angle_x > 2 * M_PI) angle_x -= 2 * M_PI; else if (angle_x < 0) angle_x += 2 * M_PI;
                // if (angle_y > 2 * M_PI) angle_y -= 2 * M_PI; else if (angle_y < 0) angle_y += 2 * M_PI;

                last_mouse_x = ev.mouse.x;
                last_mouse_y = ev.mouse.y;
                redraw = true; // Need to redraw because angles changed
            }
        }


        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(30, 30, 30));

            // 1. Apply transformations to vertices (using current angle_x, angle_y)
            for (int i = 0; i < 4; ++i) {
                Point3D p = original_vertices[i];
                p = rotate_x(p, angle_x);
                p = rotate_y(p, angle_y);
                // p = rotate_z_corrected(p, angle_z); // If you add Z rotation
                transformed_vertices[i] = p;
            }

            // 2. For each face: calculate normal, lighting, and avg_z
            for (int i = 0; i < 4; ++i) {
                Point3D v0 = transformed_vertices[faces[i].v_idx[0]];
                Point3D v1 = transformed_vertices[faces[i].v_idx[1]];
                Point3D v2 = transformed_vertices[faces[i].v_idx[2]];

                Point3D edge1 = vec_subtract(v1, v0);
                Point3D edge2 = vec_subtract(v2, v0);
                faces[i].normal = vec_normalize(vec_cross_product(edge1, edge2));

                float dot_nl = vec_dot_product(faces[i].normal, light_direction);
                float light_val = ambient_light_intensity + diffuse_light_intensity * fmax(0.0f, dot_nl);
                light_val = fmin(1.0f, light_val);

                float r, g, b;
                al_unmap_rgb_f(tetra_base_color, &r, &g, &b);
                faces[i].current_color = al_map_rgb_f(r * light_val, g * light_val, b * light_val);

                faces[i].avg_z = (v0.z + v1.z + v2.z) / 3.0f;
            }

            // 3. Sort faces by average Z
            qsort(faces, 4, sizeof(Face), compare_faces);

            // 4. Project and draw sorted faces
            for (int i = 0; i < 4; ++i) {
                Point3D v_3d_0 = transformed_vertices[faces[i].v_idx[0]];
                Point3D v_3d_1 = transformed_vertices[faces[i].v_idx[1]];
                Point3D v_3d_2 = transformed_vertices[faces[i].v_idx[2]];

                float x0 = v_3d_0.x + SCREEN_W / 2.0f;
                float y0 = -v_3d_0.y + SCREEN_H / 2.0f;
                float x1 = v_3d_1.x + SCREEN_W / 2.0f;
                float y1 = -v_3d_1.y + SCREEN_H / 2.0f;
                float x2 = v_3d_2.x + SCREEN_W / 2.0f;
                float y2 = -v_3d_2.y + SCREEN_H / 2.0f;

                al_draw_filled_triangle(x0, y0, x1, y1, x2, y2, faces[i].current_color);
            }

            if (font) {
                al_draw_text(font, al_map_rgb(255, 255, 255), 10, 10, 0, "Drag mouse to rotate. ESC to exit.");
            }

            al_flip_display();
        }
    }

    // Cleanup
    if (font) al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_event_queue(event_queue);
    al_destroy_display(display);
    al_shutdown_primitives_addon();
    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    al_uninstall_mouse(); // Uninstall mouse
    al_uninstall_keyboard();

    return 0;
}