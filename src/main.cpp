// NeonWM - Simple Version (No wlroots dependency)
// A beautiful OpenGL window manager for Wayland

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

#include <cmath>
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// ============================================================================
// CONFIGURATION - Edit these values!
// ============================================================================
namespace Config {
    // Colors (RGBA 0.0-1.0)
    constexpr float ACTIVE_BORDER[] = {0.4f, 0.75f, 1.0f, 1.0f};   // Cyan glow
    constexpr float INACTIVE_BORDER[] = {0.2f, 0.2f, 0.3f, 0.5f};
    constexpr float BACKGROUND[] = {0.05f, 0.05f, 0.08f, 1.0f};    // Dark
    
    // Visual effects
    constexpr float ANIMATION_SPEED = 0.18f;
    constexpr float GLOW_RADIUS = 25.0f;
    constexpr float GLOW_INTENSITY = 0.9f;
    constexpr float CORNER_RADIUS = 14.0f;
    
    // Tiling
    constexpr int GAP_SIZE = 12;
    constexpr float MASTER_RATIO = 0.58f;
    
    // Performance
    constexpr int TARGET_FPS = 60;
}

// ============================================================================
// SHADERS
// ============================================================================
const char* VERTEX_SHADER = R"(
#version 300 es
precision highp float;

layout(location = 0) in vec2 position;
out vec2 v_pos;

uniform mat4 projection;
uniform vec4 rect;

void main() {
    v_pos = position;
    vec2 pos = rect.xy + position * rect.zw;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

const char* FRAGMENT_SHADER = R"(
#version 300 es
precision highp float;

in vec2 v_pos;
out vec4 fragColor;

uniform vec4 color;
uniform vec4 rect;
uniform float cornerRadius;
uniform int isGlow;
uniform float glowIntensity;

float roundedBox(vec2 p, vec2 b, float r) {
    vec2 d = abs(p - b * 0.5) - b * 0.5 + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

void main() {
    vec2 size = rect.zw;
    vec2 pos = v_pos * size;
    
    float dist = roundedBox(pos, size, cornerRadius);
    
    if (isGlow == 1) {
        float glow = exp(-abs(dist) * 0.08) * glowIntensity;
        fragColor = vec4(color.rgb, glow * color.a);
    } else {
        if (dist > 0.0) discard;
        float edge = smoothstep(1.0, 0.0, dist);
        fragColor = vec4(color.rgb, color.a * edge);
    }
}
)";

// ============================================================================
// MATH UTILITIES
// ============================================================================
struct Vec2 { float x, y; };
struct Vec4 { float x, y, z, w; };
struct Rect { float x, y, w, h; };

class Mat4 {
public:
    float m[16];
    
    Mat4() {
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    
    static Mat4 ortho(float l, float r, float b, float t) {
        Mat4 mat;
        mat.m[0] = 2.0f / (r - l);
        mat.m[5] = 2.0f / (t - b);
        mat.m[10] = -1.0f;
        mat.m[12] = -(r + l) / (r - l);
        mat.m[13] = -(t + b) / (t - b);
        return mat;
    }
};

// ============================================================================
// WINDOW CLASS
// ============================================================================
class Window {
public:
    Rect geometry;
    Rect target;
    bool is_focused = false;
    float spawn_anim = 0.0f;
    float focus_anim = 0.0f;
    
    void update(float dt) {
        // Smooth lerp to target
        geometry.x += (target.x - geometry.x) * Config::ANIMATION_SPEED;
        geometry.y += (target.y - geometry.y) * Config::ANIMATION_SPEED;
        geometry.w += (target.w - geometry.w) * Config::ANIMATION_SPEED;
        geometry.h += (target.h - geometry.h) * Config::ANIMATION_SPEED;
        
        spawn_anim = std::min(1.0f, spawn_anim + dt * 3.0f);
        focus_anim += ((is_focused ? 1.0f : 0.0f) - focus_anim) * 0.15f;
    }
};

// ============================================================================
// RENDERER
// ============================================================================
class Renderer {
    GLuint program;
    GLuint vao, vbo;
    GLint u_proj, u_rect, u_color, u_corner, u_isGlow, u_glowIntensity;
    int width = 1920, height = 1080;
    
    GLuint compile_shader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            fprintf(stderr, "Shader compilation failed: %s\n", log);
        }
        return shader;
    }
    
public:
    bool init() {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        u_proj = glGetUniformLocation(program, "projection");
        u_rect = glGetUniformLocation(program, "rect");
        u_color = glGetUniformLocation(program, "color");
        u_corner = glGetUniformLocation(program, "cornerRadius");
        u_isGlow = glGetUniformLocation(program, "isGlow");
        u_glowIntensity = glGetUniformLocation(program, "glowIntensity");
        
        float vertices[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
        };
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        return true;
    }
    
    void set_viewport(int w, int h) {
        width = w;
        height = h;
        glViewport(0, 0, w, h);
    }
    
    void begin_frame() {
        glClearColor(Config::BACKGROUND[0], Config::BACKGROUND[1],
                     Config::BACKGROUND[2], Config::BACKGROUND[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program);
        glBindVertexArray(vao);
        
        Mat4 proj = Mat4::ortho(0, width, height, 0);
        glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
    }
    
    void draw_rect(Rect r, const float* color, bool glow = false, float glow_int = 1.0f) {
        glUniform4f(u_rect, r.x, r.y, r.w, r.h);
        glUniform4fv(u_color, 1, color);
        glUniform1f(u_corner, Config::CORNER_RADIUS);
        glUniform1i(u_isGlow, glow ? 1 : 0);
        glUniform1f(u_glowIntensity, glow_int);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    void draw_window(Window* win) {
        Rect r = win->geometry;
        
        // Draw glow
        if (win->focus_anim > 0.01f) {
            float g = Config::GLOW_RADIUS * win->focus_anim;
            Rect glow = {r.x - g, r.y - g, r.w + g * 2, r.h + g * 2};
            draw_rect(glow, Config::ACTIVE_BORDER, true, 
                     Config::GLOW_INTENSITY * win->focus_anim);
        }
        
        // Draw border
        const float* color = win->is_focused ? 
            Config::ACTIVE_BORDER : Config::INACTIVE_BORDER;
        draw_rect(r, color);
    }
    
    void end_frame() {
        glFlush();
    }
};

// ============================================================================
// WINDOW MANAGER
// ============================================================================
class WindowManager {
    std::vector<std::unique_ptr<Window>> windows;
    Window* focused = nullptr;
    Renderer renderer;
    int screen_w = 1920, screen_h = 1080;
    
public:
    bool init() {
        return renderer.init();
    }
    
    void set_size(int w, int h) {
        screen_w = w;
        screen_h = h;
        renderer.set_viewport(w, h);
        retile();
    }
    
    void add_window() {
        auto win = std::make_unique<Window>();
        win->geometry = {100, 100, 800, 600};
        win->target = win->geometry;
        windows.push_back(std::move(win));
        retile();
    }
    
    void close_focused() {
        if (!focused) return;
        auto it = std::find_if(windows.begin(), windows.end(),
            [this](auto& w) { return w.get() == focused; });
        if (it != windows.end()) {
            windows.erase(it);
            focused = windows.empty() ? nullptr : windows.back().get();
            retile();
        }
    }
    
    void retile() {
        if (windows.empty()) return;
        
        for (auto& w : windows) w->is_focused = false;
        
        if (windows.size() == 1) {
            windows[0]->target = {
                (float)Config::GAP_SIZE,
                (float)Config::GAP_SIZE,
                (float)(screen_w - Config::GAP_SIZE * 2),
                (float)(screen_h - Config::GAP_SIZE * 2)
            };
            windows[0]->is_focused = true;
            focused = windows[0].get();
        } else {
            float mw = screen_w * Config::MASTER_RATIO - Config::GAP_SIZE * 1.5f;
            float sw = screen_w * (1.0f - Config::MASTER_RATIO) - Config::GAP_SIZE * 1.5f;
            float sh = (screen_h - Config::GAP_SIZE * windows.size()) / (windows.size() - 1);
            
            windows[0]->target = {
                (float)Config::GAP_SIZE,
                (float)Config::GAP_SIZE,
                mw,
                (float)(screen_h - Config::GAP_SIZE * 2)
            };
            windows[0]->is_focused = true;
            focused = windows[0].get();
            
            for (size_t i = 1; i < windows.size(); i++) {
                windows[i]->target = {
                    mw + Config::GAP_SIZE * 2,
                    Config::GAP_SIZE + (sh + Config::GAP_SIZE) * (i - 1),
                    sw,
                    sh
                };
            }
        }
    }
    
    void update(float dt) {
        for (auto& win : windows) {
            win->update(dt);
        }
    }
    
    void render() {
        renderer.begin_frame();
        for (auto& win : windows) {
            renderer.draw_window(win.get());
        }
        renderer.end_frame();
    }
    
    void handle_key(uint32_t key) {
        if (key == KEY_ENTER) add_window();
        else if (key == KEY_Q) close_focused();
        else if (key == KEY_ESC) exit(0);
    }
};

// ============================================================================
// WAYLAND SETUP
// ============================================================================
struct WaylandContext {
    wl_display* display = nullptr;
    wl_compositor* compositor = nullptr;
    wl_surface* surface = nullptr;
    wl_egl_window* egl_window = nullptr;
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    int width = 1920, height = 1080;
};

static void registry_handler(void* data, wl_registry* registry,
                            uint32_t name, const char* interface, uint32_t version) {
    auto* ctx = (WaylandContext*)data;
    
    if (strcmp(interface, "wl_compositor") == 0) {
        ctx->compositor = (wl_compositor*)wl_registry_bind(
            registry, name, &wl_compositor_interface, 1);
    }
}

static void registry_remover(void*, wl_registry*, uint32_t) {}

static const wl_registry_listener registry_listener = {
    registry_handler,
    registry_remover
};

bool init_wayland(WaylandContext& ctx) {
    ctx.display = wl_display_connect(nullptr);
    if (!ctx.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return false;
    }
    
    wl_registry* registry = wl_display_get_registry(ctx.display);
    wl_registry_add_listener(registry, &registry_listener, &ctx);
    wl_display_roundtrip(ctx.display);
    
    if (!ctx.compositor) {
        fprintf(stderr, "Compositor not available\n");
        return false;
    }
    
    ctx.surface = wl_compositor_create_surface(ctx.compositor);
    
    // Init EGL
    ctx.egl_display = eglGetDisplay((EGLNativeDisplayType)ctx.display);
    eglInitialize(ctx.egl_display, nullptr, nullptr);
    
    EGLint attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(ctx.egl_display, attribs, &config, 1, &num_config);
    
    EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    ctx.egl_context = eglCreateContext(ctx.egl_display, config, 
                                       EGL_NO_CONTEXT, ctx_attribs);
    
    ctx.egl_window = wl_egl_window_create(ctx.surface, ctx.width, ctx.height);
    ctx.egl_surface = eglCreateWindowSurface(ctx.egl_display, config,
                                             (EGLNativeWindowType)ctx.egl_window, nullptr);
    
    eglMakeCurrent(ctx.egl_display, ctx.egl_surface, ctx.egl_surface, ctx.egl_context);
    
    return true;
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("🌟 NeonWM Starting...\n");
    
    WaylandContext wayland;
    if (!init_wayland(wayland)) {
        return 1;
    }
    
    WindowManager wm;
    if (!wm.init()) {
        return 1;
    }
    
    wm.set_size(wayland.width, wayland.height);
    wm.add_window();
    
    printf("✅ NeonWM Ready!\n");
    printf("Keys: Enter=new window, Q=close, ESC=quit\n");
    
    // Main loop
    float dt = 1.0f / Config::TARGET_FPS;
    while (true) {
        wl_display_dispatch_pending(wayland.display);
        
        wm.update(dt);
        wm.render();
        
        eglSwapBuffers(wayland.egl_display, wayland.egl_surface);
        usleep(16666); // ~60 FPS
    }
    
    return 0;
}
