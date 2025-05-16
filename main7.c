#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <stdarg.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCREEN_W 800
#define SCREEN_H 600
#define FPS 60.0
#define MODEL_VIEW_SIZE 150.0f
#define MOUSE_SENSITIVITY 0.005f
#define LOG_FILE "stl_viewer.log"
#define DEFAULT_STL_PATH "C:/Users/User/source/repos/計算機程式設計final project/my_model.stl"

FILE* g_log_file = NULL;

// --- Logging Function (app_log) ---
void app_log(bool also_to_console, const char* prefix, const char* format, ...) {
    if (!g_log_file && strcmp(prefix, "FATAL") != 0) {
        if (also_to_console) {
            va_list args_console; va_start(args_console, format);
            if (strcmp(prefix, "ERROR") == 0 || strcmp(prefix, "WARN") == 0 || strcmp(prefix, "FATAL") == 0) {
                fprintf(stderr, "[NO_LOG_FILE] [%s] ", prefix); vfprintf(stderr, format, args_console); fprintf(stderr, "\n");
            }
            else { printf("[NO_LOG_FILE] [%s] ", prefix); vprintf(format, args_console); printf("\n"); }
            va_end(args_console);
        } return;
    }
    char buffer[2048]; char timestamp[64]; va_list args;
    time_t now = time(NULL); strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    va_start(args, format); vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    if (g_log_file) { fprintf(g_log_file, "[%s] [%s] %s\n", timestamp, prefix, buffer); fflush(g_log_file); }
    if (also_to_console) {
        if (strcmp(prefix, "ERROR") == 0 || strcmp(prefix, "WARN") == 0 || strcmp(prefix, "FATAL") == 0) {
            fprintf(stderr, "[%s] %s\n", prefix, buffer);
        }
        else { printf("[%s] %s\n", prefix, buffer); }
    }
}

// --- Structure Definitions (Order is Important!) ---
typedef struct {
    float x, y, z;
} Point3D; // For general 3D points/vectors not needing color

typedef struct {
    float x, y, z;
    ALLEGRO_COLOR color; // Color for this vertex (for gradient)
} Vertex; // For model vertices that store color

typedef struct {
    int v_idx[3];
    float avg_z;
    Point3D normal; // Face normal uses Point3D
} Face;

typedef struct {
    float w, x, y, z;
} Quaternion;

// --- Global Variables ---
Vertex* original_vertices = NULL;
Vertex* transformed_vertices = NULL;
Face* faces = NULL;
int num_vertices = 0;
int num_faces = 0;

Point3D light_direction; // Uses Point3D
float ambient_light_intensity = 0.3f;
float diffuse_light_intensity = 0.7f;

bool is_dragging = false;
int last_mouse_x = 0;
int last_mouse_y = 0;

Quaternion g_orientation; // Global orientation quaternion

// --- Quaternion functions ---
Quaternion quaternion_identity() { return (Quaternion) { 1.0f, 0.0f, 0.0f, 0.0f }; }
Quaternion quaternion_normalize(Quaternion q) {
    float mag = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (mag == 0.0f || isnan(mag) || isinf(mag)) return quaternion_identity();
    return (Quaternion) { q.w / mag, q.x / mag, q.y / mag, q.z / mag };
}
Quaternion quaternion_multiply(Quaternion q1, Quaternion q2) {
    Quaternion r;
    r.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
    r.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
    r.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
    r.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
    return r;
}
Quaternion quaternion_from_axis_angle(Point3D axis, float angle_rad) { // Takes Point3D for axis
    Quaternion q; float half_angle = angle_rad * 0.5f; float s = sinf(half_angle);
    q.w = cosf(half_angle); q.x = axis.x * s; q.y = axis.y * s; q.z = axis.z * s;
    return q;
}
void quaternion_to_rotation_matrix(Quaternion q, float matrix[3][3]) {
    q = quaternion_normalize(q);
    float xx = q.x * q.x; float xy = q.x * q.y; float xz = q.x * q.z; float xw = q.x * q.w;
    float yy = q.y * q.y; float yz = q.y * q.z; float yw = q.y * q.w;
    float zz = q.z * q.z; float zw = q.z * q.w;
    matrix[0][0] = 1.0f - 2.0f * (yy + zz); matrix[0][1] = 2.0f * (xy - zw); matrix[0][2] = 2.0f * (xz + yw);
    matrix[1][0] = 2.0f * (xy + zw); matrix[1][1] = 1.0f - 2.0f * (xx + zz); matrix[1][2] = 2.0f * (yz - xw);
    matrix[2][0] = 2.0f * (xz - yw); matrix[2][1] = 2.0f * (yz + xw); matrix[2][2] = 1.0f - 2.0f * (xx + yy);
}
// Apply matrix to Vertex (only coords, color is preserved)
Vertex apply_rotation_matrix_to_vertex(float matrix[3][3], Vertex v_orig) {
    Vertex result;
    result.x = matrix[0][0] * v_orig.x + matrix[0][1] * v_orig.y + matrix[0][2] * v_orig.z;
    result.y = matrix[1][0] * v_orig.x + matrix[1][1] * v_orig.y + matrix[1][2] * v_orig.z;
    result.z = matrix[2][0] * v_orig.x + matrix[2][1] * v_orig.y + matrix[2][2] * v_orig.z;
    result.color = v_orig.color;
    return result;
}


// --- Vector Math (using Point3D) ---
static Point3D vec_subtract(Point3D a, Point3D b) { return (Point3D) { a.x - b.x, a.y - b.y, a.z - b.z }; }
static Point3D vec_cross_product(Point3D a, Point3D b) { return (Point3D) { a.y* b.z - a.z * b.y, a.z* b.x - a.x * b.z, a.x* b.y - a.y * b.x }; }
static float vec_dot_product(Point3D a, Point3D b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float vec_magnitude(Point3D v_pt) { // Renamed param to avoid conflict if v is Vertex
    return sqrtf(v_pt.x * v_pt.x + v_pt.y * v_pt.y + v_pt.z * v_pt.z);
}
static Point3D vec_normalize(Point3D v_pt) { // Renamed param
    float mag = vec_magnitude(v_pt);
    if (mag == 0.0f || isnan(mag) || isinf(mag)) return (Point3D) { 0, 0, 0 };
    return (Point3D) { v_pt.x / mag, v_pt.y / mag, v_pt.z / mag };
}

// --- Color Interpolation ---
float lerp(float a, float b, float t) { return a + t * (b - a); }
ALLEGRO_COLOR color_lerp(ALLEGRO_COLOR c1, ALLEGRO_COLOR c2, float t) {
    float r1, g1, b1, a1; float r2, g2, b2, a2;
    al_unmap_rgba_f(c1, &r1, &g1, &b1, &a1); al_unmap_rgba_f(c2, &r2, &g2, &b2, &a2);
    return al_map_rgba_f(lerp(r1, r2, t), lerp(g1, g2, t), lerp(b1, b2, t), lerp(a1, a2, t));
}


static void cleanup_model_data() {
    app_log(false, "DEBUG", "Cleaning up model data.");
    if (original_vertices) free(original_vertices);
    if (transformed_vertices) free(transformed_vertices);
    if (faces) free(faces);
    original_vertices = NULL; transformed_vertices = NULL; faces = NULL;
    num_vertices = 0; num_faces = 0;
}

static bool load_stl_ascii(const char* filename) {
    app_log(true, "INFO", "Attempting to load STL file: %s", filename);
    FILE* file = fopen(filename, "r");
    if (!file) { app_log(true, "ERROR", "Could not open STL file '%s'. Check path and permissions.", filename); return false; }

    char line[256]; int current_face_idx = 0; int current_vertex_idx = 0; num_faces = 0;
    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = line; while (*trimmed_line == ' ' || *trimmed_line == '\t') trimmed_line++;
        if (strncmp(trimmed_line, "facet normal", 12) == 0) num_faces++;
    }
    if (num_faces == 0) { app_log(true, "ERROR", "No facets found in STL file '%s'.", filename); fclose(file); return false; }
    app_log(false, "DEBUG", "Found %d facets (first pass).", num_faces);

    num_vertices = num_faces * 3;
    original_vertices = (Vertex*)malloc(num_vertices * sizeof(Vertex));
    transformed_vertices = (Vertex*)malloc(num_vertices * sizeof(Vertex));
    faces = (Face*)malloc(num_faces * sizeof(Face));
    if (!original_vertices || !transformed_vertices || !faces) {
        app_log(true, "ERROR", "Memory allocation failed for model data.", num_faces, num_vertices); // Corrected arg count
        cleanup_model_data(); fclose(file); return false;
    }
    app_log(false, "DEBUG", "Memory allocated for %d vertices and %d faces.", num_vertices, num_faces);

    rewind(file);
    Point3D min_coord_pt = { FLT_MAX,FLT_MAX,FLT_MAX }; // Use Point3D for these bounds
    Point3D max_coord_pt = { -FLT_MAX,-FLT_MAX,-FLT_MAX };
    current_face_idx = 0; current_vertex_idx = 0; int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++; char* trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t' || *trimmed_line == '\r' || *trimmed_line == '\n') { if (*trimmed_line == '\0') break; trimmed_line++; }
        if (*trimmed_line == '\0') continue;
        if (strncmp(trimmed_line, "vertex", 6) == 0) {
            if (current_vertex_idx < num_vertices) {
                // Read into a temporary Point3D first, then assign to Vertex.x,y,z
                Point3D temp_p;
                int items_read = sscanf(trimmed_line, "vertex %f %f %f", &temp_p.x, &temp_p.y, &temp_p.z);
                if (items_read == 3) {
                    original_vertices[current_vertex_idx].x = temp_p.x;
                    original_vertices[current_vertex_idx].y = temp_p.y;
                    original_vertices[current_vertex_idx].z = temp_p.z;
                    // original_vertices[current_vertex_idx].color will be set later

                    if (temp_p.x < min_coord_pt.x) min_coord_pt.x = temp_p.x;
                    if (temp_p.y < min_coord_pt.y) min_coord_pt.y = temp_p.y;
                    if (temp_p.z < min_coord_pt.z) min_coord_pt.z = temp_p.z;
                    if (temp_p.x > max_coord_pt.x) max_coord_pt.x = temp_p.x;
                    if (temp_p.y > max_coord_pt.y) max_coord_pt.y = temp_p.y;
                    if (temp_p.z > max_coord_pt.z) max_coord_pt.z = temp_p.z;
                    current_vertex_idx++;
                }
                else { app_log(true, "WARN", "Line %d: Failed to parse 3 floats for vertex from: '%s'", line_num, trimmed_line); }
            }
            else { app_log(true, "WARN", "Line %d: Read more vertices than allocated (%d). Ignoring: '%s'", line_num, num_vertices, trimmed_line); }
        }
        else if (strncmp(trimmed_line, "endfacet", 8) == 0) {
            if (current_face_idx < num_faces) {
                if (current_vertex_idx >= (current_face_idx * 3) + 3) {
                    faces[current_face_idx].v_idx[0] = (current_face_idx * 3) + 0;
                    faces[current_face_idx].v_idx[1] = (current_face_idx * 3) + 1;
                    faces[current_face_idx].v_idx[2] = (current_face_idx * 3) + 2;
                }
                else { app_log(true, "ERROR", "Line %d: Not enough vertices (%d) to form face %d. STL might be corrupt.", line_num, current_vertex_idx, current_face_idx); }
                current_face_idx++;
            }
            else { app_log(true, "WARN", "Line %d: Read more 'endfacet' than counted. Ignoring.", line_num); }
        }
    }
    fclose(file);
    if (current_face_idx != num_faces) { app_log(true, "WARN", "Final face count mismatch. Expected %d, processed %d.", num_faces, current_face_idx); num_faces = current_face_idx; }
    if (num_faces == 0) { app_log(true, "ERROR", "STL parsing resulted in zero valid faces."); cleanup_model_data(); return false; }

    Point3D center_pt = { 0,0,0 }; float scale_factor = 1.0f; // Use Point3D for center
    if (num_vertices > 0 && isfinite(min_coord_pt.x) && isfinite(max_coord_pt.x)) {
        center_pt.x = (min_coord_pt.x + max_coord_pt.x) / 2.0f;
        center_pt.y = (min_coord_pt.y + max_coord_pt.y) / 2.0f;
        center_pt.z = (min_coord_pt.z + max_coord_pt.z) / 2.0f;
        float extent_x = max_coord_pt.x - min_coord_pt.x; float extent_y = max_coord_pt.y - min_coord_pt.y; float extent_z = max_coord_pt.z - min_coord_pt.z;
        float max_extent = fmaxf(extent_x, fmaxf(extent_y, extent_z));
        if (max_extent > 1e-6f) scale_factor = MODEL_VIEW_SIZE / max_extent;
    }
    else if (num_vertices > 0) { app_log(true, "WARN", "Could not determine model bounds accurately."); }

    float min_y_orig = FLT_MAX; float max_y_orig = -FLT_MAX;
    for (int i = 0; i < num_vertices; ++i) {
        original_vertices[i].x = (original_vertices[i].x - center_pt.x) * scale_factor;
        original_vertices[i].y = (original_vertices[i].y - center_pt.y) * scale_factor;
        original_vertices[i].z = (original_vertices[i].z - center_pt.z) * scale_factor;
        if (original_vertices[i].y < min_y_orig) min_y_orig = original_vertices[i].y;
        if (original_vertices[i].y > max_y_orig) max_y_orig = original_vertices[i].y;
    }

    ALLEGRO_COLOR color_bottom = al_map_rgb(0, 0, 255); ALLEGRO_COLOR color_top = al_map_rgb(0, 255, 0);
    for (int i = 0; i < num_vertices; ++i) {
        float t = 0.5f;
        if ((max_y_orig - min_y_orig) > 1e-6f) { t = (original_vertices[i].y - min_y_orig) / (max_y_orig - min_y_orig); }
        t = fminf(1.0f, fmaxf(0.0f, t));
        original_vertices[i].color = color_lerp(color_bottom, color_top, t);
    }
    app_log(false, "DEBUG", "Vertex colors calculated based on Y range [%.2f, %.2f]", min_y_orig, max_y_orig);
    app_log(true, "INFO", "Successfully processed STL: %s, Faces: %d, Vertices: %d", filename, num_faces, num_vertices);
    light_direction = vec_normalize((Point3D) { 0.5f, 0.5f, -1.0f });
    return true;
}
static int init_allegro() { /* ... same ... */
    app_log(false, "DEBUG", "Initializing Allegro...");
    if (!al_init()) { app_log(true, "ERROR", "Failed to initialize Allegro core!"); return -1; }
    if (!al_install_keyboard()) { app_log(true, "ERROR", "Failed to initialize the keyboard!"); return -1; }
    if (!al_install_mouse()) { app_log(true, "ERROR", "Failed to initialize the mouse!"); return -1; }
    if (!al_init_primitives_addon()) { app_log(true, "ERROR", "Failed to initialize primitives addon!"); return -1; }
    if (!al_init_font_addon()) { app_log(true, "WARN", "Failed to initialize font addon."); }
    if (!al_init_ttf_addon()) { app_log(true, "WARN", "Failed to initialize ttf addon."); }
    app_log(false, "DEBUG", "Allegro initialized successfully.");
    return 0;
}
int compare_faces(const void* a, const void* b) { /* ... same ... */
    const Face* faceA = (const Face*)a; const Face* faceB = (const Face*)b;
    if (faceA->avg_z < faceB->avg_z) return 1; if (faceA->avg_z > faceB->avg_z) return -1; return 0;
}

int main(int argc, char** argv) {
    g_log_file = fopen(LOG_FILE, "w");
    if (!g_log_file) { app_log(true, "FATAL", "Could not open log file %s. Exiting.", LOG_FILE); return 1; }
    app_log(true, "INFO", "Application started. Log file: %s", LOG_FILE);
    g_orientation = quaternion_identity();

    ALLEGRO_DISPLAY* display = NULL; ALLEGRO_EVENT_QUEUE* event_queue = NULL;
    ALLEGRO_TIMER* timer = NULL; ALLEGRO_FONT* font = NULL;

    const char* stl_filename = NULL;
    if (argc < 2) {
        stl_filename = DEFAULT_STL_PATH;
        app_log(true, "INFO", "No command line argument for STL file. Using default: %s", stl_filename);
    }
    else {
        stl_filename = argv[1];
        app_log(false, "DEBUG", "STL filename from args: %s", stl_filename);
    }

    if (init_allegro() != 0) { fclose(g_log_file); return -1; }
    display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) { app_log(true, "ERROR", "Failed to create display!"); /* full cleanup */ fclose(g_log_file); return -1; }
    timer = al_create_timer(1.0 / FPS);
    if (!timer) { app_log(true, "ERROR", "Failed to create timer!"); /* full cleanup */ fclose(g_log_file); return -1; }
    event_queue = al_create_event_queue();
    if (!event_queue) { app_log(true, "ERROR", "Failed to create event_queue!"); /* full cleanup */ fclose(g_log_file); return -1; }
    font = al_load_ttf_font("arial.ttf", 18, 0);
    if (!font) { app_log(true, "WARN", "Failed to load 'arial.ttf'. Trying built-in font."); font = al_create_builtin_font(); }
    if (!font) { app_log(true, "ERROR", "Failed to load any font."); }
    else { app_log(false, "DEBUG", "Font loaded."); }
    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_mouse_event_source());

    if (!load_stl_ascii(stl_filename)) { app_log(true, "INFO", "Exiting due to STL load failure."); /* full cleanup */ fclose(g_log_file); return -1; }

    bool redraw = true; al_start_timer(timer); bool running = true;
    app_log(false, "DEBUG", "Entering main loop.");

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
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) { running = false; }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev.mouse.button == 1) { is_dragging = true; last_mouse_x = ev.mouse.x; last_mouse_y = ev.mouse.y; }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
            if (ev.mouse.button == 1) { is_dragging = false; }
        }
        else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES || ev.type == ALLEGRO_EVENT_MOUSE_WARPED) {
            if (is_dragging) {
                int mouse_dx = ev.mouse.x - last_mouse_x; int mouse_dy = ev.mouse.y - last_mouse_y;
                float delta_angle_y_view = -mouse_dx * MOUSE_SENSITIVITY; Point3D view_up_axis = { 0, 1, 0 };
                Quaternion q_rot_around_view_y = quaternion_from_axis_angle(view_up_axis, delta_angle_y_view);
                float delta_angle_x_view = -mouse_dy * MOUSE_SENSITIVITY; Point3D view_right_axis = { 1, 0, 0 };
                Quaternion q_rot_around_view_x = quaternion_from_axis_angle(view_right_axis, delta_angle_x_view);
                Quaternion screen_space_delta_rotation = quaternion_multiply(q_rot_around_view_y, q_rot_around_view_x);
                g_orientation = quaternion_multiply(screen_space_delta_rotation, g_orientation);
                g_orientation = quaternion_normalize(g_orientation);
                last_mouse_x = ev.mouse.x; last_mouse_y = ev.mouse.y; redraw = true;
            }
        }

        if (redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            if (num_faces == 0 || num_vertices == 0) {
                al_clear_to_color(al_map_rgb(30, 30, 30)); if (font)al_draw_text(font, al_map_rgb(255, 0, 0), SCREEN_W / 2.f, SCREEN_H / 2.f, ALLEGRO_ALIGN_CENTER, "Model empty."); al_flip_display(); continue;
            }
            al_clear_to_color(al_map_rgb(30, 30, 30));

            float rotation_matrix[3][3];
            quaternion_to_rotation_matrix(g_orientation, rotation_matrix);

            for (int i = 0; i < num_vertices; ++i) {
                transformed_vertices[i] = apply_rotation_matrix_to_vertex(rotation_matrix, original_vertices[i]);
            }

            for (int i = 0; i < num_faces; ++i) {
                if (faces[i].v_idx[0] >= num_vertices || faces[i].v_idx[1] >= num_vertices || faces[i].v_idx[2] >= num_vertices ||
                    faces[i].v_idx[0] < 0 || faces[i].v_idx[1] < 0 || faces[i].v_idx[2] < 0) {
                    faces[i].avg_z = FLT_MAX; continue; // Skip if invalid
                }
                Vertex v_t0 = transformed_vertices[faces[i].v_idx[0]];
                Vertex v_t1 = transformed_vertices[faces[i].v_idx[1]];
                Vertex v_t2 = transformed_vertices[faces[i].v_idx[2]];

                Point3D p0 = { v_t0.x, v_t0.y, v_t0.z }; Point3D p1 = { v_t1.x, v_t1.y, v_t1.z }; Point3D p2 = { v_t2.x, v_t2.y, v_t2.z };
                Point3D edge1 = vec_subtract(p1, p0); Point3D edge2 = vec_subtract(p2, p0);
                faces[i].normal = vec_normalize(vec_cross_product(edge1, edge2));
                faces[i].avg_z = (v_t0.z + v_t1.z + v_t2.z) / 3.0f;
            }

            if (num_faces > 0) qsort(faces, num_faces, sizeof(Face), compare_faces);

            for (int i = 0; i < num_faces; ++i) {
                if (faces[i].v_idx[0] >= num_vertices || faces[i].v_idx[1] >= num_vertices || faces[i].v_idx[2] >= num_vertices) continue;

                Vertex v_draw[3]; // Get the vertices for this sorted face
                v_draw[0] = transformed_vertices[faces[i].v_idx[0]];
                v_draw[1] = transformed_vertices[faces[i].v_idx[1]];
                v_draw[2] = transformed_vertices[faces[i].v_idx[2]];

                // Use the pre-calculated normal for this face for lighting
                float light_val_draw = ambient_light_intensity + diffuse_light_intensity * fmaxf(0.0f, vec_dot_product(faces[i].normal, light_direction));
                light_val_draw = fminf(1.0f, fmaxf(0.0f, light_val_draw));

                ALLEGRO_VERTEX tri_verts_allegro[3]; // Allegro's vertex type
                for (int k = 0; k < 3; ++k) {
                    tri_verts_allegro[k].x = v_draw[k].x + SCREEN_W / 2.0f;
                    tri_verts_allegro[k].y = -v_draw[k].y + SCREEN_H / 2.0f;
                    tri_verts_allegro[k].z = 0;

                    float r_base, g_base, b_base, a_base;
                    al_unmap_rgba_f(v_draw[k].color, &r_base, &g_base, &b_base, &a_base);
                    tri_verts_allegro[k].color = al_map_rgba_f(
                        r_base * light_val_draw, g_base * light_val_draw, b_base * light_val_draw, a_base
                    );
                }
                al_draw_prim(tri_verts_allegro, NULL, 0, 0, 3, ALLEGRO_PRIM_TRIANGLE_LIST);
            }

            if (font) {
                char info_text[128];
                snprintf(info_text, sizeof(info_text), "Faces: %d. Verts: %d. Gradient. ESC exit.", num_faces, num_vertices);
                al_draw_text(font, al_map_rgb(255, 255, 255), 10, 10, 0, info_text);
            }
            al_flip_display();
        }
    }
    app_log(false, "DEBUG", "Exiting main loop.");

    // No need to free draw_buffer as it's not used in the final drawing loop

    app_log(false, "DEBUG", "Starting cleanup sequence.");
    cleanup_model_data();
    if (font) al_destroy_font(font); if (event_queue) al_destroy_event_queue(event_queue);
    if (timer) al_destroy_timer(timer); if (display) al_destroy_display(display);
    al_shutdown_ttf_addon(); al_shutdown_font_addon(); al_shutdown_primitives_addon();
    al_uninstall_mouse(); al_uninstall_keyboard();
    app_log(true, "INFO", "Application terminated normally.");
    if (g_log_file) fclose(g_log_file);
    return 0;
}