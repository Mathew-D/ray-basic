#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

// ---------------- shared helpers ----------------

static Vector2 g_uiMousePos{};
static bool g_uiMousePosSet = false;
static Camera2D g_uiCamera{};
static bool g_uiCameraSet = false;

void SetUiMousePosition(Vector2 pos) {
    g_uiMousePos = pos;
    g_uiMousePosSet = true;
}

static Vector2 widgetMousePos() {
    return g_uiMousePosSet ? g_uiMousePos : GetMousePosition();
}

Vector2 UiMousePosition() {
    return widgetMousePos();
}

void UiSyncMouse(const Camera2D& cam) {
    // GetMousePosition() returns logical (window) units while the camera
    // works in render pixels, so scale by the DPI factor. Umbriel's
    // fractional-scale input path delivers pointer coords an extra 1/dpi
    // smaller, so the factor is applied twice there.
    Vector2 mouse = GetMousePosition();
    float dpi = GetWindowScaleDPI().x;
    mouse.x *= dpi;
    mouse.y *= dpi;
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (desktop != nullptr && strstr(desktop, "umbriel") != nullptr) {
        mouse.x *= dpi;
        mouse.y *= dpi;
    }
    SetUiMousePosition(GetScreenToWorld2D(mouse, cam));
    g_uiCamera = cam;
    g_uiCameraSet = true;
}

Camera2D UiBeginDesignFrame(float designW, float designH) {
    // Inside BeginMode2D raylib works in render pixels (the projection is
    // framebuffer-sized and screenScale is not reapplied), so the camera
    // must be built from GetRenderWidth/Height — NOT the logical
    // GetScreenWidth/Height — when FLAG_WINDOW_HIGHDPI is set.
    Camera2D cam{};
    cam.offset = {GetRenderWidth() / 2.0f, GetRenderHeight() / 2.0f};
    cam.target = {designW / 2.0f, designH / 2.0f};
    cam.zoom = fminf(GetRenderWidth() / designW,
                     GetRenderHeight() / designH);
    UiSyncMouse(cam);
    return cam;
}

void UiDrawGrid(float gridSize, Color color, float designW, float designH) {
    if (gridSize <= 0.0f || !g_uiCameraSet) return;

    // Default to the camera's design space: UiBeginDesignFrame centers
    // the target on it, so target*2 is the design size.
    if (designW <= 0.0f) designW = g_uiCamera.target.x * 2.0f;
    if (designH <= 0.0f) designH = g_uiCamera.target.y * 2.0f;

    // Keep lines ~1 screen pixel and labels ~16 screen pixels regardless
    // of the camera zoom, like the Rust draw_grid.
    float lw = 1.0f / g_uiCamera.zoom;
    int labelSize = std::max(8, (int)lroundf(16.0f / g_uiCamera.zoom));

    for (float x = 0.0f; x <= designW; x += gridSize) {
        DrawLineEx({x, 0.0f}, {x, designH}, lw, color);
        DrawText(std::to_string((int)x).c_str(),
                 (int)(x + 2.0f), 2, labelSize, color);
    }
    for (float y = 0.0f; y <= designH; y += gridSize) {
        DrawLineEx({0.0f, y}, {designW, y}, lw, color);
        DrawText(std::to_string((int)y).c_str(),
                 2, (int)(y + 2.0f), labelSize, color);
    }
}

namespace {

Color lerpColor(Color a, Color b, float t) {
    return Color{
        (unsigned char)(a.r * (1.0f - t) + b.r * t),
        (unsigned char)(a.g * (1.0f - t) + b.g * t),
        (unsigned char)(a.b * (1.0f - t) + b.b * t),
        255,
    };
}

Color shadeColor(Color c, float factor) {
    return Color{
        (unsigned char)std::clamp(c.r * factor, 0.0f, 255.0f),
        (unsigned char)std::clamp(c.g * factor, 0.0f, 255.0f),
        (unsigned char)std::clamp(c.b * factor, 0.0f, 255.0f),
        c.a,
    };
}

// raylib wants roundness as a fraction of half the smaller side.
float roundnessFor(float radius, float w, float h) {
    float halfMin = fminf(w, h) * 0.5f;
    if (halfMin <= 0.0f || radius <= 0.0f) return 0.0f;
    return std::clamp(radius / halfMin, 0.0f, 1.0f);
}

void drawRoundRect(float x, float y, float w, float h, float radius, Color color) {
    DrawRectangleRounded({x, y, w, h}, roundnessFor(radius, w, h), 16, color);
}

void drawRoundRectBorder(float x, float y, float w, float h, float radius,
                         float thickness, Color color) {
    DrawRectangleRoundedLinesEx({x, y, w, h}, roundnessFor(radius, w, h), 16,
                                thickness, color);
}

std::vector<std::string> splitTextLines(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = text.find('\n', start);
        if (pos == std::string::npos) {
            out.push_back(text.substr(start));
            break;
        }
        out.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

// Tracks press-origin for click detection: returns true only when the
// press began while hovered and the release happens while still hovered.
// `armed` must persist between frames (per-widget member).
bool trackClick(bool hovered, bool enabled, bool visible, bool& armed) {
    if (!visible || !enabled) {
        armed = false;
        return false;
    }
    // Arm only on the press edge — a press that started elsewhere and
    // dragged onto this widget must not count.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered) armed = true;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        bool clicked = armed && hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        armed = false;
        return clicked;
    }
    return false;
}

// If `path` doesn't exist, generate a placeholder PNG so widgets can be
// built from plain paths with no binary assets checked into the repo.
// Paths are relative to the process working directory — run the game
// from the project root (e.g. via `just run`).
// The placeholder color is derived from the path, so different assets
// (e.g. normal vs hover textures) get different colors.
const char* ensureImageFile(const char* path) {
    if (path == nullptr || path[0] == '\0' || FileExists(path)) return path;

    unsigned h = 2166136261u;
    for (const char* p = path; *p; ++p) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
    }
    Color bg{(unsigned char)(80 + h % 176),
             (unsigned char)(80 + (h >> 8) % 176),
             (unsigned char)(80 + (h >> 16) % 176), 255};
    Image img = GenImageColor(64, 64, bg);
    ImageDrawCircle(&img, 32, 32, 20, WHITE);
    ExportImage(img, path);
    UnloadImage(img);
    return path;
}

// ---- UTF-8 helpers (TextInput stores text as UTF-8) ----

std::string utf8Encode(unsigned cp) {
    std::string s;
    if (cp < 0x80) {
        s += (char)cp;
    } else if (cp < 0x800) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xF0 | (cp >> 18));
        s += (char)(0x80 | ((cp >> 12) & 0x3F));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
    return s;
}

// Byte index of the start of the char containing/preceding index i.
size_t prevCharBoundary(const std::string& s, size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) --i;
    return i;
}

// Byte index just past the char starting at/after index i.
size_t nextCharBoundary(const std::string& s, size_t i) {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) ++i;
    return i;
}

// ---- collision helpers ----

// World-space AABB of an ImageView, accounting for rotation about center.
Rectangle worldAabb(const ImageView& v) {
    Vector2 p = v.pos();
    Vector2 s = v.size();
    if (v.getAngle() == 0.0f) return {p.x, p.y, s.x, s.y};

    float cx = p.x + s.x / 2.0f, cy = p.y + s.y / 2.0f;
    float c = cosf(v.getAngle()), sn = sinf(v.getAngle());
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    const float corners[4][2] = {{-s.x/2,-s.y/2},{s.x/2,-s.y/2},{s.x/2,s.y/2},{-s.x/2,s.y/2}};
    for (auto& k : corners) {
        float wx = cx + k[0]*c - k[1]*sn;
        float wy = cy + k[0]*sn + k[1]*c;
        minX = std::min(minX, wx); maxX = std::max(maxX, wx);
        minY = std::min(minY, wy); maxY = std::max(maxY, wy);
    }
    return {minX, minY, maxX - minX, maxY - minY};
}

// Map a world point to a texel coordinate; false if outside the view.
bool worldToTexel(const ImageView& v, float wx, float wy, int& tx, int& ty) {
    Vector2 p = v.pos();
    Vector2 s = v.size();
    float cx = p.x + s.x / 2.0f, cy = p.y + s.y / 2.0f;
    float dx = wx - cx, dy = wy - cy;
    // inverse of the draw rotation (angle is radians, y-down screen space)
    float c = cosf(v.getAngle()), sn = sinf(v.getAngle());
    float lx = dx * c + dy * sn + s.x / 2.0f;
    float ly = -dx * sn + dy * c + s.y / 2.0f;
    if (lx < 0.0f || ly < 0.0f || lx >= s.x || ly >= s.y) return false;
    Vector2 ts = v.textureSize();
    tx = (int)(lx / s.x * ts.x);
    ty = (int)(ly / s.y * ts.y);
    return true;
}

bool imageOpaqueAt(const ImageView& v, float wx, float wy) {
    int tx, ty;
    if (!worldToTexel(v, wx, wy, tx, ty)) return false;
    const auto& mask = v.collisionMask();
    if (mask.empty()) return true;   // fully opaque image
    return mask[(size_t)ty * (size_t)v.textureSize().x + (size_t)tx] != 0;
}

} // namespace

// ---------------- Label ----------------

Label::Label(const std::string& t, float x_, float y_, int size)
    : text(t), x(x_), y(y_), fontSize(size) {
    recalc();
}

Label& Label::withColors(Color fg, std::optional<Color> bg) {
    foreground = fg;
    background = bg;
    return *this;
}

Label& Label::withFont(Font* f) {
    font = f;
    recalc();
    return *this;
}

Label& Label::withRound(float radius) {
    cornerRadius = radius;
    return *this;
}

Label& Label::withBorder(Color color, float thickness) {
    border = true;
    borderColor = color;
    borderThickness = thickness;
    return *this;
}

Label& Label::withFixedSize(float w, float h) {
    fixedWidth = w;
    fixedHeight = h;
    recalc();
    return *this;
}

Label& Label::withAlignment(TextAlign align) {
    textAlign = align;
    return *this;
}

Label& Label::withVisibility(bool v) {
    visible = v;
    return *this;
}

void Label::setText(const std::string& t) {
    text = t;
    recalc();
}

void Label::setPosition(float nx, float ny) {
    x = nx;
    y = ny;
}

void Label::setFontSize(int size) {
    fontSize = size;
    recalc();
}

void Label::setVisible(bool v) { visible = v; }
bool Label::toggleVisibility() { visible = !visible; return visible; }

Vector2 Label::measure(const std::string& s) const {
    if (font && font->texture.id != 0) return MeasureTextEx(*font, s.c_str(), (float)fontSize, 1.0f);
    return Vector2{(float)MeasureText(s.c_str(), fontSize), (float)fontSize};
}

float Label::getWidth() const {
    if (fixedWidth) return *fixedWidth;
    return maxLineWidth + 10.0f;
}

float Label::getHeight() const {
    if (fixedHeight) return *fixedHeight;
    return totalHeight;
}

std::vector<std::string> Label::chunkWord(const std::string& word, float maxWidth) const {
    std::vector<std::string> chunks;
    std::string current;
    for (char ch : word) {
        std::string candidate = current.empty() ? std::string(1, ch) : current + ch;
        if (measure(candidate).x <= maxWidth) {
            current = candidate;
        } else {
            if (!current.empty()) chunks.push_back(current);
            current = std::string(1, ch);
        }
    }
    if (!current.empty()) chunks.push_back(current);
    return chunks;
}

std::vector<std::pair<std::string, float>> Label::wrapText(float maxWidth) const {
    std::vector<std::pair<std::string, float>> wrapped;
    if (maxWidth <= 0.0f) return wrapped;

    for (const std::string& sourceLine : splitTextLines(text)) {
        // split on whitespace, keeping it simple (spaces/tabs)
        std::vector<std::string> words;
        size_t i = 0;
        while (i < sourceLine.size()) {
            while (i < sourceLine.size() && isspace((unsigned char)sourceLine[i])) i++;
            size_t start = i;
            while (i < sourceLine.size() && !isspace((unsigned char)sourceLine[i])) i++;
            if (i > start) words.push_back(sourceLine.substr(start, i - start));
        }
        if (words.empty()) {
            wrapped.push_back({"", 0.0f});
            continue;
        }

        std::string currentLine;
        for (const std::string& word : words) {
            if (measure(word).x > maxWidth) {
                if (!currentLine.empty()) {
                    wrapped.push_back({currentLine, measure(currentLine).x});
                    currentLine.clear();
                }
                auto chunks = chunkWord(word, maxWidth);
                for (size_t c = 0; c < chunks.size(); c++) {
                    if (c + 1 == chunks.size()) {
                        currentLine = chunks[c];
                    } else {
                        wrapped.push_back({chunks[c], measure(chunks[c]).x});
                    }
                }
                continue;
            }
            std::string candidate = currentLine.empty() ? word : currentLine + " " + word;
            if (measure(candidate).x <= maxWidth || currentLine.empty()) {
                currentLine = candidate;
            } else {
                wrapped.push_back({currentLine, measure(currentLine).x});
                currentLine = word;
            }
        }
        if (!currentLine.empty()) wrapped.push_back({currentLine, measure(currentLine).x});
    }
    return wrapped;
}

void Label::recalc() {
    lines.clear();
    lineWidths.clear();
    maxLineWidth = 0.0f;

    float lineHeight = fontSize * lineSpacing;

    if (fixedWidth) {
        auto wrapped = wrapText(*fixedWidth - 10.0f);
        for (auto& [line, w] : wrapped) {
            lines.push_back(line);
            lineWidths.push_back(w);
            maxLineWidth = std::max(maxLineWidth, w);
        }
    } else {
        lines = splitTextLines(text);
        for (const std::string& line : lines) {
            float w = measure(line).x;
            lineWidths.push_back(w);
            maxLineWidth = std::max(maxLineWidth, w);
        }
    }

    if (!fixedHeight) totalHeight = (float)lines.size() * lineHeight;
}

void Label::draw() const {
    if (!visible) return;

    float lineHeight = fontSize * lineSpacing;
    float w = fixedWidth.value_or(maxLineWidth + 10.0f);
    float h = fixedHeight.value_or(totalHeight);

    float bgX = x - 5.0f;
    float bgY = y - 5.0f;

    if (background) {
        if (cornerRadius > 0.0f) {
            drawRoundRect(bgX, bgY, w, h, cornerRadius, *background);
        } else {
            DrawRectangle((int)bgX, (int)bgY, (int)w, (int)h, *background);
        }
    }

    if (border) {
        if (cornerRadius > 0.0f) {
            drawRoundRectBorder(bgX, bgY, w, h, cornerRadius, borderThickness, borderColor);
        } else {
            DrawRectangleLinesEx({bgX, bgY, w, h}, borderThickness, borderColor);
        }
    }

    for (size_t i = 0; i < lines.size(); i++) {
        float ty = y + (float)i * lineHeight;
        if (fixedHeight && ty > bgY + h) break;

        float tx = x;
        if (fixedWidth) {
            float textArea = std::max(*fixedWidth - 10.0f, 0.0f);
            switch (textAlign) {
                case TextAlign::Left:   tx = x; break;
                case TextAlign::Center: tx = x + std::max((textArea - lineWidths[i]) / 2.0f, 0.0f); break;
                case TextAlign::Right:  tx = x + std::max(textArea - lineWidths[i], 0.0f); break;
            }
        }

        if (font && font->texture.id != 0) {
            DrawTextEx(*font, lines[i].c_str(), {tx, ty}, (float)fontSize, 1.0f, foreground);
        } else {
            DrawText(lines[i].c_str(), (int)tx, (int)ty, fontSize, foreground);
        }
    }
}

// ---------------- TextButton ----------------

TextButton::TextButton(float x_, float y_, float w_, float h_,
                       const std::string& t, Color normal, Color hover, int size)
    : width(w_), height(h_), normalColor(normal), hoverColor(hover), fontSize(size),
      x(x_), y(y_), text(t) {
    offColor = lerpColor(normalColor, GRAY, 0.5f);
    textColor = WHITE;
    hoverTextColor = textColor;
}

TextButton& TextButton::withTextColor(Color color) {
    textColor = color;
    hoverTextColor = color;
    return *this;
}

TextButton& TextButton::withHoverTextColor(Color color) {
    hoverTextColor = color;
    return *this;
}

TextButton& TextButton::withFont(Font* f) {
    font = f;
    return *this;
}

TextButton& TextButton::withRound(float radius) {
    cornerRadius = radius;
    return *this;
}

TextButton& TextButton::withBorder(Color color, float thickness) {
    border = true;
    borderColor = color;
    borderThickness = thickness;
    return *this;
}

TextButton& TextButton::withAlignment(TextAlign align) {
    textAlign = align;
    return *this;
}

TextButton& TextButton::withVerticalAlignment(VerticalAlign align) {
    verticalAlign = align;
    return *this;
}

void TextButton::setText(const std::string& t) { text = t; }

void TextButton::updatePosition(float nx, float ny, float w, float h) {
    x = nx;
    y = ny;
    if (w >= 0.0f) width = w;
    if (h >= 0.0f) height = h;
}

Vector2 TextButton::measure(const std::string& s) const {
    if (font && font->texture.id != 0) return MeasureTextEx(*font, s.c_str(), (float)fontSize, 1.0f);
    return Vector2{(float)MeasureText(s.c_str(), fontSize), (float)fontSize};
}

std::vector<std::string> TextButton::splitLines() const {
    return splitTextLines(text);
}

bool TextButton::hovered() const {
    Vector2 m = widgetMousePos();
    // If the background is fully transparent, only the text area is clickable.
    if (normalColor.a == 0) {
        auto lines = splitLines();
        float lineHeight = fontSize * 1.2f;
        float totalH = (float)lines.size() * lineHeight;
        float maxW = 0.0f;
        for (auto& l : lines) maxW = std::max(maxW, measure(l).x);
        float top = y;
        switch (verticalAlign) {
            case VerticalAlign::Top:    top = y; break;
            case VerticalAlign::Center: top = y + (height - totalH) / 2.0f; break;
            case VerticalAlign::Bottom: top = y + height - totalH; break;
        }
        Rectangle textRect;
        switch (textAlign) {
            case TextAlign::Left:   textRect = {x + 8.0f, top, maxW, totalH}; break;
            case TextAlign::Center: textRect = {x + (width - maxW) / 2.0f, top, maxW, totalH}; break;
            case TextAlign::Right:  textRect = {x + width - maxW - 8.0f, top, maxW, totalH}; break;
        }
        return CheckCollisionPointRec(m, textRect);
    }
    return CheckCollisionPointRec(m, bounds());
}

void TextButton::draw() {
    if (!visible) return;

    bool isHovered = hovered();
    bool isPressed = isHovered && enabled && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color buttonColor = enabled ? (isHovered ? hoverColor : normalColor) : offColor;
    if (isPressed) buttonColor = shadeColor(buttonColor, 0.8f);

    bool transparent = buttonColor.a == 0;
    float pressOffset = isPressed ? 1.0f : 0.0f;
    float dx = x, dy = y + pressOffset;

    // shadow
    if (!transparent) {
        Color shadow{0, 0, 0, 51};
        if (cornerRadius > 0.0f) drawRoundRect(dx, dy + 2.0f, width, height, cornerRadius, shadow);
        else DrawRectangle((int)dx, (int)(dy + 2.0f), (int)width, (int)height, shadow);
    }

    // body
    if (cornerRadius > 0.0f) {
        drawRoundRect(dx, dy, width, height, cornerRadius, buttonColor);
        if (!transparent) {
            // subtle top highlight / bottom shade for depth
            Color hi{255, 255, 255, (unsigned char)(isPressed ? 25 : 70)};
            Color lo{0, 0, 0, (unsigned char)(isPressed ? 45 : 66)};
            drawRoundRectBorder(dx + 1.0f, dy + 1.0f, width - 2.0f, height - 2.0f,
                                cornerRadius - 1.0f, 1.0f, hi);
            drawRoundRectBorder(dx, dy, width, height, cornerRadius, 1.0f, lo);
        }
        if (border) drawRoundRectBorder(dx, dy, width, height, cornerRadius, borderThickness, borderColor);
    } else {
        DrawRectangle((int)dx, (int)dy, (int)width, (int)height, buttonColor);
        if (!transparent) {
            Color hi{255, 255, 255, (unsigned char)(isPressed ? 25 : 75)};
            Color lo{0, 0, 0, (unsigned char)(isPressed ? 50 : 70)};
            DrawLineEx({(float)dx + 1, (float)dy + 1}, {(float)(dx + width - 1), (float)dy + 1}, 1.0f, hi);
            DrawLineEx({(float)dx + 1, (float)dy + 1}, {(float)dx + 1, (float)(dy + height - 1)}, 1.0f, hi);
            DrawLineEx({(float)dx, (float)(dy + height)}, {(float)(dx + width), (float)(dy + height)}, 1.0f, lo);
            DrawLineEx({(float)(dx + width), (float)dy}, {(float)(dx + width), (float)(dy + height)}, 1.0f, lo);
        }
        if (border) DrawRectangleLinesEx({dx, dy, width, height}, borderThickness, borderColor);
    }

    // text
    Color currentTextColor = enabled ? (isHovered ? hoverTextColor : textColor)
                                     : Color{textColor.r, textColor.g, textColor.b, 128};
    auto lines = splitLines();
    float lineHeight = fontSize * 1.2f;
    float totalH = (float)lines.size() * lineHeight;
    float top = y;
    switch (verticalAlign) {
        case VerticalAlign::Top:    top = y + pressOffset; break;
        case VerticalAlign::Center: top = y + (height - totalH) / 2.0f + pressOffset; break;
        case VerticalAlign::Bottom: top = y + height - totalH + pressOffset; break;
    }
    for (size_t i = 0; i < lines.size(); i++) {
        float tw = measure(lines[i]).x;
        float tx = x;
        switch (textAlign) {
            case TextAlign::Left:   tx = x + 8.0f; break;
            case TextAlign::Center: tx = x + (width - tw) / 2.0f; break;
            case TextAlign::Right:  tx = x + width - tw - 8.0f; break;
        }
        float ty = top + (float)i * lineHeight;
        if (font && font->texture.id != 0) DrawTextEx(*font, lines[i].c_str(), {tx, ty}, (float)fontSize, 1.0f, currentTextColor);
        else      DrawText(lines[i].c_str(), (int)tx, (int)ty, fontSize, currentTextColor);
    }
}

bool TextButton::clicked() {
    return trackClick(hovered(), enabled, visible, clickArmed);
}

bool TextButton::click() {
    draw();
    return clicked();
}

bool TextButton::held() const {
    return enabled && visible && hovered() && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

// ---------------- ImageButton ----------------

void ImageButton::load(const char* path, const char* hoverPath) {
    texture = LoadTexture(ensureImageFile(path));
    hoverTexture = LoadTexture(ensureImageFile(hoverPath));

    if (hasImage) UnloadImage(image);
    image = LoadImage(ensureImageFile(path));
    hasImage = true;
}

ImageButton::ImageButton(const char* path, const char* hoverPath,
                         float x_, float y_, float w_, float h_)
    : x(x_), y(y_), width(w_), height(h_) {
    load(path, hoverPath);
}

ImageButton::~ImageButton() {
    if (texture.id) UnloadTexture(texture);
    if (hoverTexture.id) UnloadTexture(hoverTexture);
    if (hasImage) UnloadImage(image);
}

void ImageButton::setImage(const char* path, const char* hoverPath) {
    if (texture.id) UnloadTexture(texture);
    if (hoverTexture.id) UnloadTexture(hoverTexture);
    load(path, hoverPath);
}

bool ImageButton::inside(float mx, float my) const {
    return CheckCollisionPointRec({mx, my}, {x, y, width, height});
}

bool ImageButton::pixelHovered(float mx, float my) const {
    if (!hasImage || image.data == nullptr) return inside(mx, my);
    int tx = (int)((mx - x) * (float)image.width / width);
    int ty = (int)((my - y) * (float)image.height / height);
    if (tx < 0 || ty < 0 || tx >= image.width || ty >= image.height) return false;
    // raylib images are RGBA8 by default
    int idx = (ty * image.width + tx) * 4 + 3;
    return ((const unsigned char*)image.data)[idx] > 0;
}

void ImageButton::draw() {
    if (!visible) return;
    Vector2 m = widgetMousePos();
    bool hov = inside(m.x, m.y) && enabled && pixelHovered(m.x, m.y);
    Texture2D& t = hov ? hoverTexture : texture;
    Color tint = enabled ? WHITE : Color{150, 150, 150, 255};
    Rectangle dst{x, y, width, height};
    Rectangle src{0, 0, (float)t.width, (float)t.height};
    DrawTexturePro(t, src, dst, {0, 0}, 0.0f, tint);
}

bool ImageButton::clicked() {
    Vector2 m = widgetMousePos();
    bool hov = inside(m.x, m.y) && pixelHovered(m.x, m.y);
    return trackClick(hov, enabled, visible, clickArmed);
}

bool ImageButton::click() {
    draw();
    return clicked();
}

// ---------------- ImageView ----------------

static Texture2D loadTextureOrEmpty(const char* path, bool& empty) {
    if (path == nullptr || path[0] == '\0') {
        Image img = GenImageColor(1, 1, BLANK);
        Texture2D t = LoadTextureFromImage(img);
        UnloadImage(img);
        empty = true;
        return t;
    }
    empty = false;
    return LoadTexture(path);
}

ImageView::ImageView(const char* path, float x_, float y_, float w, float h,
                     bool stretch, float zoom_)
    : x(x_), y(y_), width(w), height(h), stretchEnabled(stretch),
      zoom(fmaxf(zoom_, 0.1f)), filename(path ? path : "") {
    const char* p = ensureImageFile(path);
    texture = loadTextureOrEmpty(p, empty);
    buildCollisionMask(p);
}

void ImageView::buildCollisionMask(const char* path) {
    alphaMask.clear();
    if (path == nullptr || path[0] == '\0' || !FileExists(path)) return;

    Image img = LoadImage(path);
    if (img.data == nullptr || img.width <= 0 || img.height <= 0) {
        UnloadImage(img);
        return;
    }

    alphaMask.assign((size_t)img.width * (size_t)img.height, 0);
    bool anyTransparent = false;
    if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        const unsigned char* px = (const unsigned char*)img.data;
        for (size_t i = 0; i < alphaMask.size(); i++) {
            unsigned char a = px[i * 4 + 3];
            alphaMask[i] = a > 128 ? 1 : 0;
            if (a <= 128) anyTransparent = true;
        }
    } else {
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                Color c = GetImageColor(img, x, y);
                alphaMask[(size_t)y * img.width + x] = c.a > 128 ? 1 : 0;
                if (c.a <= 128) anyTransparent = true;
            }
        }
    }
    UnloadImage(img);

    if (!anyTransparent) alphaMask.clear();  // solid image needs no mask
}

ImageView::~ImageView() {
    if (texture.id) UnloadTexture(texture);
}

void ImageView::setTexture(const char* path) {
    if (texture.id) UnloadTexture(texture);
    const char* p = ensureImageFile(path);
    texture = loadTextureOrEmpty(p, empty);
    filename = path ? path : "";
    buildCollisionMask(p);
}

Vector2 ImageView::size() const {
    float w = stretchEnabled ? width : (float)texture.width;
    float h = stretchEnabled ? height : (float)texture.height;
    return {w * zoom, h * zoom};
}

void ImageView::draw() const {
    Vector2 s = size();
    Rectangle src{0, 0, (float)texture.width, (float)texture.height};
    // DrawTexturePro treats dstrec.x/y as the position of `origin` (the
    // rotation pivot), NOT the rect's top-left — it subtracts origin even
    // at rotation 0. Passing the center as both dst position and origin
    // keeps the top-left at (x, y) and rotates around the middle.
    Rectangle dst{x + s.x / 2.0f, y + s.y / 2.0f, s.x, s.y};
    DrawTexturePro(texture, src, dst, {s.x / 2.0f, s.y / 2.0f},
                   angle * RAD2DEG, WHITE);
}

// ---------------- CheckCollision ----------------

bool CheckCollision(const ImageView& a, const ImageView& b, int skipPixels) {
    if (skipPixels < 1) skipPixels = 1;

    Rectangle ba = worldAabb(a), bb = worldAabb(b);
    float x0 = std::max(ba.x, bb.x), y0 = std::max(ba.y, bb.y);
    float x1 = std::min(ba.x + ba.width, bb.x + bb.width);
    float y1 = std::min(ba.y + ba.height, bb.y + bb.height);
    if (x1 <= x0 || y1 <= y0) return false;

    for (float y = y0; y < y1; y += skipPixels) {
        for (float x = x0; x < x1; x += skipPixels) {
            // sample pixel centers
            if (imageOpaqueAt(a, x + 0.5f, y + 0.5f) &&
                imageOpaqueAt(b, x + 0.5f, y + 0.5f)) {
                return true;
            }
        }
    }
    return false;
}

// ---------------- TextInput ----------------

TextInput::TextInput(float x_, float y_, float w, float h, int size)
    : x(x_), y(y_), width(w), height(h), fontSize(size) {}

TextInput& TextInput::withColors(Color t, Color b, Color bg, Color c) {
    textColor = t; borderColor = b; backgroundColor = bg; cursorColor = c;
    return *this;
}

TextInput& TextInput::withPrompt(const std::string& p, Color c) {
    prompt = p; promptColor = c;
    return *this;
}

TextInput& TextInput::withFont(Font* f) {
    font = f;
    return *this;
}

void TextInput::setText(const std::string& t) {
    text = t;
    cursorIndex = text.size();
    scrollX = 0.0f;
}

// Match DrawText's spacing convention: fontSize/10 (min font size 10).
float TextInput::letterSpacing() const {
    int fs = fontSize < 10 ? 10 : fontSize;
    return (float)(fs / 10);
}

Font TextInput::resolveFont() const {
    // A Font whose texture failed to load (e.g. missing .ttf file) would
    // make MeasureTextEx return zero widths — fall back to the default.
    return (font != nullptr && font->texture.id != 0) ? *font : GetFontDefault();
}

// DrawTextEx advances advanceX + spacing after EVERY glyph, so width
// measurements must add letterSpacing() per char to match.
float TextInput::textWidthUpTo(size_t byteIndex) const {
    Font f = resolveFont();
    float w = 0.0f;
    for (size_t i = 0; i < byteIndex;) {
        size_t n = nextCharBoundary(text, i);
        w += MeasureTextEx(f, text.substr(i, n - i).c_str(),
                           (float)fontSize, letterSpacing()).x + letterSpacing();
        i = n;
    }
    return w;
}

size_t TextInput::indexFromLocalX(float lx) const {
    if (lx <= 0.0f) return 0;
    Font f = resolveFont();
    float bestDist = 1e30f;
    size_t best = 0;
    float cum = 0.0f;
    for (size_t i = 0; i <= text.size();) {
        float d = fabsf(cum - lx);
        if (d < bestDist) { bestDist = d; best = i; }
        if (i >= text.size()) break;
        size_t n = nextCharBoundary(text, i);
        cum += MeasureTextEx(f, text.substr(i, n - i).c_str(),
                             (float)fontSize, letterSpacing()).x + letterSpacing();
        i = n;
    }
    return best;
}

void TextInput::update() {
    if (!enabled) {
        active_ = false;
        return;
    }

    Vector2 m = widgetMousePos();
    bool inside = m.x >= x && m.x <= x + width && m.y >= y && m.y <= y + height;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (inside != active_) {
            active_ = inside;
            cursorTimer = 0.0f;
        }
        if (active_) {
            // + scrollX: drawn text is shifted left by scrollX.
            cursorIndex = indexFromLocalX(m.x - (x + PAD) + scrollX);
            cursorTimer = 0.0f;
        }
    }

    cursorTimer += GetFrameTime();
    if (!active_) return;

    int cp;
    while ((cp = GetCharPressed()) != 0) {
        if (cp < 32 && cp != '\t') continue;   // skip control chars
        std::string s = utf8Encode((unsigned)cp);
        text.insert(cursorIndex, s);
        cursorIndex += s.size();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && cursorIndex > 0) {
        size_t p = prevCharBoundary(text, cursorIndex);
        text.erase(p, cursorIndex - p);
        cursorIndex = p;
    }
    if (IsKeyPressed(KEY_DELETE) && cursorIndex < text.size()) {
        size_t n = nextCharBoundary(text, cursorIndex);
        text.erase(cursorIndex, n - cursorIndex);
    }
    if (IsKeyPressed(KEY_LEFT) && cursorIndex > 0)
        cursorIndex = prevCharBoundary(text, cursorIndex);
    if (IsKeyPressed(KEY_RIGHT) && cursorIndex < text.size())
        cursorIndex = nextCharBoundary(text, cursorIndex);
    if (IsKeyPressed(KEY_HOME)) cursorIndex = 0;
    if (IsKeyPressed(KEY_END)) cursorIndex = text.size();
    if (IsKeyPressed(KEY_ENTER)) active_ = false;
}

void TextInput::draw() {
    DrawRectangle(x, y, width, height, backgroundColor);
    DrawRectangleLinesEx({x, y, width, height}, 1.0f, borderColor);

    Font f = resolveFont();
    float textX = x + PAD;
    float textY = y + (height - (float)fontSize) / 2.0f;
    float availW = width - 2.0f * PAD;

    // Keep the cursor inside the visible area by scrolling the text.
    float cursorTextX = textWidthUpTo(cursorIndex);
    if (cursorTextX - scrollX < 0.0f) {
        scrollX = cursorTextX;
    } else if (cursorTextX - scrollX > availW) {
        scrollX = cursorTextX - availW;
    }
    if (scrollX < 0.0f) scrollX = 0.0f;

    // Clip to the field's inner area and draw the text as one string so
    // the font's natural spacing applies (same as Label). scrollX shifts
    // it horizontally for long text.
    bool showingPrompt = text.empty() && !prompt.empty();
    const std::string& shown = showingPrompt ? prompt : text;
    Color shownColor = showingPrompt ? promptColor : textColor;
    if (!shown.empty()) {
        // NOTE: this leans on raylib internals — BeginScissorMode works
        // in logical screen pixels (it applies the DPI scale itself
        // under FLAG_WINDOW_HIGHDPI), ignores the 2D camera, and assumes
        // an unrotated camera. If raylib changes scissor semantics or a
        // rotated camera is used, this clip rect will be wrong.
        Rectangle clip{x + PAD, y, availW, height};
        if (g_uiCameraSet) {
            const Camera2D& c = g_uiCamera;
            clip.x = c.offset.x + (clip.x - c.target.x) * c.zoom;
            clip.y = c.offset.y + (clip.y - c.target.y) * c.zoom;
            clip.width *= c.zoom;
            clip.height *= c.zoom;
        }
        Vector2 dpi = GetWindowScaleDPI();
        BeginScissorMode((int)(clip.x / dpi.x), (int)(clip.y / dpi.y),
                         (int)(clip.width / dpi.x), (int)(clip.height / dpi.y));
        DrawTextEx(f, shown.c_str(), {textX - scrollX, textY},
                   (float)fontSize, letterSpacing(), shownColor);
        EndScissorMode();
    }

    // Blinking cursor while active.
    if (active_ && fmodf(cursorTimer, 1.0f) < 0.5f) {
        float cx = textX + cursorTextX - scrollX;
        DrawLineEx({cx, textY}, {cx, textY + (float)fontSize}, 1.5f, cursorColor);
    }
}
