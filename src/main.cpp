#include <wayland-server.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <cmath>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <algorithm>

// ============================================================================
// CONFIG.HPP - Your customization file!
// ============================================================================
namespace Config {
    // Animation settings
    constexpr float ANIMATION_SPEED = 0.15f;
    constexpr float WINDOW_SPAWN_SCALE = 0.8f;
    
    // Visual effects
    constexpr float GLOW_INTENSITY = 0.8f;
    constexpr float GLOW_RADIUS = 20.0f;
    constexpr float SHADOW_OPACITY = 0.6f;
    constexpr float CORNER_RADIUS = 12.0f;
    
    // Colors (RGBA)
    constexpr float ACTIVE_BORDER[] = {0.4f, 0.7f, 1.0f, 1.0f};   // Cyan glow
    constexpr float INACTIVE_BORDER[] = {0.2f, 0.2f, 0.3f, 0.5f}; // Dim gray
    constexpr float BACKGROUND[] = {0.05f, 0.05f, 0.08f, 1.0f};   // Dark purple-black
    
    // Tiling
    constexpr int GAP_SIZE = 10;
    constexpr int BORDER_WIDTH = 2;
    constexpr float MASTER_RATIO = 0.55f;
    
    // Keybindings (modifiers: MOD_SUPER = 1, MOD_SHIFT = 2, MOD_CTRL = 4, MOD_ALT = 8)
    struct KeyBind {
        uint32_t mod;
        uint32_t key;
        const char* action;
    };
    
    const KeyBind KEYBINDS[] = {
        {1, KEY_ENTER, "spawn_terminal"},    // Super + Enter
        {1, KEY_Q, "close_window"},          // Super + Q
        {1, KEY_F, "toggle_fullscreen"},     // Super + F
        {1, KEY_H, "focus_left"},            // Super + H
        {1, KEY_L, "focus_right"},           // Super + L
        {1, KEY_J, "focus_down"},            // Super + J
        {1, KEY_K, "focus_up"},              // Super + K
        {1 | 2, KEY_H, "move_left"},         // Super + Shift + H
        {1 | 2, KEY_L, "move_right"},        // Super + Shift + L
        {1 | 2, KEY_J, "move_down"},         // Super + Shift + J
        {1 | 2, KEY_K, "move_up"},           // Super + Shift + K
        {1, KEY_SPACE, "toggle_floating"},   // Super + Space
        {1 | 2, KEY_Q, "quit"},              // Super + Shift + Q
    };
}

// ============================================================================
// SHADER CODE
// ============================================================================
const char* VERTEX_SHADER = R"(
#version 300 es
precision highp float;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

out vec2 v_texcoord;
out vec2 v_position;

uniform mat4 projection;
uniform mat4 model;

void main() {
    v_texcoord = texcoord;
    v_position = position;
    gl_Position = projection * model * vec4(position, 0.0, 1.0);
}
)";

const char* FRAGMENT_SHADER = R"(
#version 300 es
precision highp float;

in vec2 v_texcoord;
in vec2 v_position;
out vec4 fragColor;

uniform vec4 color;
uniform vec4 glowColor;
uniform float glowIntensity;
uniform float cornerRadius;
uniform vec2 size;
uniform int isGlow;

float roundedBox(vec2 p, vec2 b, float r) {
    vec2 d = abs(p) - b + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

void main() {
    if (isGlow == 1) {
        vec2 center = size * 0.5;
        float dist = roundedBox(v_position - center, size * 0.5, cornerRadius);
        float glow = exp(-abs(dist) * 0.1) * glowIntensity;
        fragColor = vec4(glowColor.rgb, glow * glowColor.a);
    } else {
        vec2 center = size * 0.5;
        float dist = roundedBox(v_position - center, size * 0.5, cornerRadius);
        
        if (dist > 0.0) discard;
        
        float edge = smoothstep(1.0, 0.0, dist);
        fragColor = vec4(color.rgb, color.a * edge);
    }
}
)";

// ============================================================================
// MATH & RENDERING UTILITIES
// ============================================================================
struct Vec2 { float x, y; };
struct Vec4 { float x, y, z, w; };
struct Rect { float x, y, w, h; };

class Matrix4 {
public:
    float m[16];
    
    Matrix4() {
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    
    static Matrix4 ortho(float l, float r, float b, float t) {
        Matrix4 mat;
        mat.m[0] = 2.0f / (r - l);
        mat.m[5] = 2.0f / (t - b);
        mat.m[10] = -1.0f;
        mat.m[12] = -(r + l) / (r - l);
        mat.m[13] = -(t + b) / (t - b);
        return mat;
    }
    
    static Matrix4 translate(float x, float y) {
        Matrix4 mat;
        mat.m[12] = x;
        mat.m[13] = y;
        return mat;
    }
    
    static Matrix4 scale(float x, float y) {
        Matrix4 mat;
        mat.m[0] = x;
        mat.m[5] = y;
        return mat;
    }
};

// ============================================================================
// WINDOW CLASS
// ============================================================================
class Window {
public:
    Rect geometry;
    Rect target_geometry;
    bool is_floating = false;
    bool is_fullscreen = false;
    bool is_focused = false;
    float spawn_anim = 0.0f;
    float focus_anim = 0.0f;
    
    void update(float dt) {
        // Lerp to target geometry
        geometry.x += (target_geometry.x - geometry.x) * Config::ANIMATION_SPEED;
        geometry.y += (target_geometry.y - geometry.y) * Config::ANIMATION_SPEED;
        geometry.w += (target_geometry.w - geometry.w) * Config::ANIMATION_SPEED;
        geometry.h += (target_geometry.h - geometry.h) * Config::ANIMATION_SPEED;
        
        // Animations
        spawn_anim = std::min(1.0f, spawn_anim + dt * 3.0f);
        focus_anim += ((is_focused ? 1.0f : 0.0f) - focus_anim) * Config::ANIMATION_SPEED;
    }
};

// ============================================================================
// RENDERER
// ============================================================================
class Renderer {
    GLuint shader_program;
    GLuint vao, vbo;
    GLint u_projection, u_model, u_color, u_glowColor;
    GLint u_glowIntensity, u_cornerRadius, u_size, u_isGlow;
    
public:
    int screen_width = 1920;
    int screen_height = 1080;
    
    bool init() {
        // Compile shaders
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &VERTEX_SHADER, nullptr);
        glCompileShader(vs);
        
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &FRAGMENT_SHADER, nullptr);
        glCompileShader(fs);
        
        shader_program = glCreateProgram();
        glAttachShader(shader_program, vs);
        glAttachShader(shader_program, fs);
        glLinkProgram(shader_program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        // Get uniforms
        u_projection = glGetUniformLocation(shader_program, "projection");
        u_model = glGetUniformLocation(shader_program, "model");
        u_color = glGetUniformLocation(shader_program, "color");
        u_glowColor = glGetUniformLocation(shader_program, "glowColor");
        u_glowIntensity = glGetUniformLocation(shader_program, "glowIntensity");
        u_cornerRadius = glGetUniformLocation(shader_program, "cornerRadius");
        u_size = glGetUniformLocation(shader_program, "size");
        u_isGlow = glGetUniformLocation(shader_program, "isGlow");
        
        // Setup geometry
        float vertices[] = {
            0, 0,  0, 0,
            1, 0,  1, 0,
            1, 1,  1, 1,
            0, 0,  0, 0,
            1, 1,  1, 1,
            0, 1,  0, 1,
        };
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        return true;
    }
    
    void begin_frame() {
        glClearColor(Config::BACKGROUND[0], Config::BACKGROUND[1], 
                     Config::BACKGROUND[2], Config::BACKGROUND[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(shader_program);
        glBindVertexArray(vao);
        
        Matrix4 proj = Matrix4::ortho(0, screen_width, screen_height, 0);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, proj.m);
    }
    
    void draw_window(Window* win) {
        float scale = Config::WINDOW_SPAWN_SCALE + 
                     (1.0f - Config::WINDOW_SPAWN_SCALE) * win->spawn_anim;
        
        Rect r = win->geometry;
        float cx = r.x + r.w * 0.5f;
        float cy = r.y + r.h * 0.5f;
        
        // Draw glow
        float glow_size = Config::GLOW_RADIUS * win->focus_anim;
        Rect glow_rect = {
            r.x - glow_size, r.y - glow_size,
            r.w + glow_size * 2, r.h + glow_size * 2
        };
        
        Matrix4 glow_model = Matrix4::translate(glow_rect.x, glow_rect.y);
        Matrix4 glow_scale = Matrix4::scale(glow_rect.w, glow_rect.h);
        glUniformMatrix4fv(u_model, 1, GL_FALSE, glow_model.m);
        
        glUniform4fv(u_glowColor, 1, Config::ACTIVE_BORDER);
        glUniform1f(u_glowIntensity, Config::GLOW_INTENSITY * win->focus_anim);
        glUniform1f(u_cornerRadius, Config::CORNER_RADIUS);
        glUniform2f(u_size, glow_rect.w, glow_rect.h);
        glUniform1i(u_isGlow, 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Draw window
        Matrix4 trans = Matrix4::translate(cx, cy);
        Matrix4 sc = Matrix4::scale(r.w * scale, r.h * scale);
        Matrix4 back = Matrix4::translate(-r.w * 0.5f, -r.h * 0.5f);
        
        glUniformMatrix4fv(u_model, 1, GL_FALSE, trans.m);
        
        float* border_col = win->is_focused ? 
            (float*)Config::ACTIVE_BORDER : (float*)Config::INACTIVE_BORDER;
        glUniform4fv(u_color, 1, border_col);
        glUniform2f(u_size, r.w, r.h);
        glUniform1i(u_isGlow, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    void end_frame() {
        glFlush();
    }
};

// ============================================================================
// WINDOW MANAGER CORE
// ============================================================================
class NeonWM {
    std::vector<std::unique_ptr<Window>> windows;
    Window* focused_window = nullptr;
    Renderer renderer;
    int screen_width = 1920;
    int screen_height = 1080;
    
public:
    bool init() {
        return renderer.init();
    }
    
    void add_window() {
        auto win = std::make_unique<Window>();
        win->geometry = {100, 100, 800, 600};
        win->target_geometry = win->geometry;
        windows.push_back(std::move(win));
        retile();
    }
    
    void close_focused() {
        if (!focused_window) return;
        auto it = std::find_if(windows.begin(), windows.end(),
            [this](auto& w) { return w.get() == focused_window; });
        if (it != windows.end()) {
            windows.erase(it);
            focused_window = windows.empty() ? nullptr : windows.back().get();
            retile();
        }
    }
    
    void focus_window(int direction) {
        // direction: 0=left, 1=right, 2=up, 3=down
        if (windows.empty()) return;
        
        auto it = std::find_if(windows.begin(), windows.end(),
            [this](auto& w) { return w.get() == focused_window; });
        
        if (it != windows.end()) {
            if (direction == 0 && it != windows.begin()) --it;
            else if (direction == 1 && it + 1 != windows.end()) ++it;
            focused_window = it->get();
        }
    }
    
    void retile() {
        if (windows.empty()) return;
        
        for (auto& w : windows) w->is_focused = false;
        
        if (windows.size() == 1) {
            windows[0]->target_geometry = {
                (float)Config::GAP_SIZE,
                (float)Config::GAP_SIZE,
                (float)(screen_width - Config::GAP_SIZE * 2),
                (float)(screen_height - Config::GAP_SIZE * 2)
            };
            windows[0]->is_focused = true;
            focused_window = windows[0].get();
        } else {
            float master_w = screen_width * Config::MASTER_RATIO - Config::GAP_SIZE * 1.5f;
            float stack_w = screen_width * (1.0f - Config::MASTER_RATIO) - Config::GAP_SIZE * 1.5f;
            float stack_h = (screen_height - Config::GAP_SIZE * (windows.size())) / (windows.size() - 1);
            
            windows[0]->target_geometry = {
                (float)Config::GAP_SIZE,
                (float)Config::GAP_SIZE,
                master_w,
                (float)(screen_height - Config::GAP_SIZE * 2)
            };
            windows[0]->is_focused = true;
            focused_window = windows[0].get();
            
            for (size_t i = 1; i < windows.size(); i++) {
                windows[i]->target_geometry = {
                    master_w + Config::GAP_SIZE * 2,
                    Config::GAP_SIZE + (stack_h + Config::GAP_SIZE) * (i - 1),
                    stack_w,
                    stack_h
                };
            }
        }
    }
    
    void handle_key(uint32_t mods, uint32_t key) {
        for (const auto& bind : Config::KEYBINDS) {
            if (bind.mod == mods && bind.key == key) {
                std::string action = bind.action;
                
                if (action == "spawn_terminal") add_window();
                else if (action == "close_window") close_focused();
                else if (action == "focus_left") focus_window(0);
                else if (action == "focus_right") focus_window(1);
                else if (action == "quit") exit(0);
                
                break;
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
};

// ============================================================================
// MAIN
// ============================================================================
int main() {
    NeonWM wm;
    
    if (!wm.init()) {
        return 1;
    }
    
    // Add initial window
    wm.add_window();
    
    // Main loop (simplified - real implementation needs Wayland event loop)
    while (true) {
        wm.update(0.016f); // ~60 FPS
        wm.render();
    }
    
    return 0;
}
