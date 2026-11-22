#pragma once

#include "raylib.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Port of the rust-objects UI widgets to raylib (C++).
//
// Usage sketch:
//   Label      lbl("Hello\nWorld", 50, 100, 30);
//   lbl.withColors(WHITE, DARKGRAY);
//   lbl.draw();
//
//   TextButton btn(100, 200, 200, 60, "Click Me", BLUE, GREEN, 30);
//   btn.draw();
//   if (btn.click()) { ... }
//
//   ImageView  img("assets/pic.png", 200, 200, 100, 60, true, 1.0f);
//   img.draw();
//
//   ImageButton ibtn("assets/btn.png", "assets/btn_hover.png", 100, 300, 200, 60);
//   if (ibtn.click()) { ... }

enum class TextAlign { Left, Center, Right };
enum class VerticalAlign { Top, Center, Bottom };

// Optional override for the mouse position used by widget hit-testing.
// Call once per frame before drawing widgets — e.g. with camera-transformed
// coordinates. If never called, widgets use GetMousePosition() directly.
void SetUiMousePosition(Vector2 pos);

// Current mouse position in widget/design coordinates — the same value
// widgets use for hit-testing. Useful for custom drag/hit logic.
Vector2 UiMousePosition();

// Call once per frame before drawing widgets: returns a camera that maps
// a design space of designW x designH onto the window, centered with
// letterbox bars if the aspect ratio differs, and syncs the mouse into
// widget coordinates for hit-testing. Works in render pixels, so it is
// correct when FLAG_WINDOW_HIGHDPI is set:
//   BeginMode2D(UiBeginDesignFrame(800, 450));
//   ...widgets... EndMode2D();
Camera2D UiBeginDesignFrame(float designW, float designH);

// Converts the mouse position into widget space for the given camera and
// stores it for widget hit-testing. Only needed if you build a custom
// camera instead of UiBeginDesignFrame.
void UiSyncMouse(const Camera2D& cam);

// Debug helper: draws a coordinate grid with axis labels over the
// design space — grid coordinates match widget coordinates, so a "400"
// label is exactly where x = 400 widgets are placed. Call inside
// BeginMode2D after UiBeginDesignFrame. gridSize is in design units;
// designW/designH default to the camera's design space (pass them
// explicitly for custom cameras). The alpha channel of color controls
// visibility.
void UiDrawGrid(float gridSize, Color color,
                float designW = 0.0f, float designH = 0.0f);

class Label {
public:
    Label(const std::string& text, float x, float y, int fontSize);

    Label& withColors(Color foreground, std::optional<Color> background = std::nullopt);
    Label& withFont(Font* font);
    Label& withRound(float radius);
    Label& withBorder(Color color, float thickness);
    Label& withFixedSize(float width, float height);
    Label& withAlignment(TextAlign align);
    Label& withVisibility(bool visible);

    void setText(const std::string& text);
    void setPosition(float x, float y);
    void setFontSize(int fontSize);
    void setVisible(bool visible);
    bool toggleVisibility();

    float getX() const { return x; }
    float getY() const { return y; }
    Vector2 getPosition() const { return {x, y}; }
    float getWidth() const;
    float getHeight() const;
    int getFontSize() const { return fontSize; }
    const std::string& getText() const { return text; }
    bool isVisible() const { return visible; }

    void draw() const;

private:
    Vector2 measure(const std::string& s) const;
    std::vector<std::pair<std::string, float>> wrapText(float maxWidth) const;
    std::vector<std::string> chunkWord(const std::string& word, float maxWidth) const;
    void recalc();

    std::string text;
    float x, y;
    int fontSize;
    Color foreground = BLACK;
    std::optional<Color> background;
    float lineSpacing = 1.2f;
    Font* font = nullptr;
    float cornerRadius = 0.0f;
    bool border = false;
    Color borderColor = BLACK;
    float borderThickness = 1.0f;
    bool visible = true;
    std::optional<float> fixedWidth, fixedHeight;
    TextAlign textAlign = TextAlign::Left;

    // cached
    std::vector<std::string> lines;
    std::vector<float> lineWidths;
    float maxLineWidth = 0.0f;
    float totalHeight = 0.0f;
};

class TextButton {
public:
    TextButton(float x, float y, float width, float height,
               const std::string& text, Color normalColor, Color hoverColor,
               int fontSize);

    TextButton& withTextColor(Color color);
    TextButton& withHoverTextColor(Color color);
    TextButton& withFont(Font* font);
    TextButton& withRound(float radius);
    TextButton& withBorder(Color color, float thickness);
    TextButton& withAlignment(TextAlign align);
    TextButton& withVerticalAlignment(VerticalAlign align);

    void setText(const std::string& text);
    void updatePosition(float x, float y, float width = -1.0f, float height = -1.0f);

    float getX() const { return x; }
    float getY() const { return y; }
    Vector2 getPosition() const { return {x, y}; }
    const std::string& getText() const { return text; }

    bool enabled = true;
    bool visible = true;
    float width, height;
    Color normalColor, hoverColor, textColor, hoverTextColor;
    int fontSize;

    // Draws the button using the current mouse state.
    void draw();
    // Returns true if the button was just clicked (does not draw).
    // Requires the press to have started on the button.
    bool clicked();
    // Draws the button, then returns true if it was just clicked.
    bool click();
    // True while the left mouse button is held down over the button —
    // useful for "hold to repeat" interactions like moving an object.
    bool held() const;

private:
    Vector2 measure(const std::string& s) const;
    std::vector<std::string> splitLines() const;
    bool hovered() const;
    Rectangle bounds() const { return {x, y, width, height}; }

    float x, y;
    bool clickArmed = false;
    std::string text;
    Color offColor;
    Font* font = nullptr;
    float cornerRadius = 0.0f;
    bool border = false;
    Color borderColor = BLACK;
    float borderThickness = 1.0f;
    TextAlign textAlign = TextAlign::Center;
    VerticalAlign verticalAlign = VerticalAlign::Center;
};

class ImageButton {
public:
    ImageButton(const char* texturePath, const char* hoverTexturePath,
                float x, float y, float width, float height);
    ~ImageButton();
    ImageButton(const ImageButton&) = delete;
    ImageButton& operator=(const ImageButton&) = delete;

    void setBounds(float x, float y, float width, float height) { this->x = x; this->y = y; this->width = width; this->height = height; }
    void setImage(const char* texturePath, const char* hoverTexturePath);

    void draw();
    // Returns true if the button was just clicked (does not draw).
    // Requires the press to have started on the button.
    bool clicked();
    // Draws the button, then returns true if it was just clicked.
    bool click();

    bool enabled = true;
    bool visible = true;
    float x = 0, y = 0, width = 0, height = 0;

private:
    bool pixelHovered(float mx, float my) const;
    bool inside(float mx, float my) const;
    void load(const char* path, const char* hoverPath);

    bool clickArmed = false;
    Texture2D texture{};
    Texture2D hoverTexture{};
    Image image{};   // kept for per-pixel alpha hit-testing
    bool hasImage = false;
};

class ImageView {
public:
    ImageView(const char* path, float x, float y, float width, float height,
              bool stretchEnabled = true, float zoom = 1.0f);
    ~ImageView();
    ImageView(const ImageView&) = delete;
    ImageView& operator=(const ImageView&) = delete;

    void draw() const;
    void setTexture(const char* path);

    Vector2 pos() const { return {x, y}; }
    Vector2 size() const;
    Vector2 textureSize() const { return {(float)texture.width, (float)texture.height}; }
    void setPosition(Vector2 p) { x = p.x; y = p.y; }
    void setAngle(float a) { angle = a; }
    float getAngle() const { return angle; }
    float getX() const { return x; }
    void setX(float v) { x = v; }
    float getY() const { return y; }
    void setY(float v) { y = v; }
    const std::string& getFilename() const { return filename; }

    void enableStretch() { stretchEnabled = true; }
    void disableStretch() { stretchEnabled = false; }
    void toggleStretch() { stretchEnabled = !stretchEnabled; }
    bool isStretchEnabled() const { return stretchEnabled; }
    void setStretch(bool enabled) { stretchEnabled = enabled; }

    void setZoom(float z) { zoom = std::fmax(z, 0.1f); }
    void zoomIn(float amount) { zoom = std::fmax(zoom + amount, 0.1f); }
    void zoomOut(float amount) { zoom = std::fmax(zoom - amount, 0.1f); }
    float getZoomLevel() const { return zoom; }
    void resetZoom() { zoom = 1.0f; }

    bool isEmpty() const { return empty; }
    bool isCollidable() const { return !empty; }

    // Alpha mask for collision detection: one byte per texel at texture
    // resolution (nonzero = opaque). Empty if the image is fully opaque
    // or failed to load — CheckCollision treats it as fully solid.
    const std::vector<uint8_t>& collisionMask() const { return alphaMask; }

private:
    Texture2D texture{};
    bool empty = false;
    float x, y, width, height;
    bool stretchEnabled;
    float zoom;
    float angle = 0.0f;
    std::string filename;
    std::vector<uint8_t> alphaMask;
    void buildCollisionMask(const char* path);
};

// Pixel-perfect collision between two ImageViews — respects alpha
// transparency and rotation around each view's center.
// skipPixels strides the overlap sampling (1 = every pixel; higher is
// faster but less precise). Both views must be in the same coordinate
// space (e.g. the design space inside BeginMode2D).
bool CheckCollision(const ImageView& a, const ImageView& b, int skipPixels = 1);

// Single-line text input. Call update() once per frame in the update
// phase (handles focus, clicks, typing, cursor keys) and draw() in the
// draw phase. This is a subset of the Rust original — no selection,
// clipboard, multiline, or key repeat:
//   TextInput input(20, 265, 360, 36, 20);
//   input.withColors(WHITE, GRAY, {30,30,30,255}, WHITE)
//        .withPrompt("Type here...", GRAY);
//   ...
//   input.update();          // update phase
//   input.draw();            // draw phase
//   std::string text = input.getText();
class TextInput {
public:
    TextInput(float x, float y, float width, float height, int fontSize);

    TextInput& withColors(Color text, Color border, Color background, Color cursor);
    TextInput& withPrompt(const std::string& prompt, Color color = GRAY);
    TextInput& withFont(Font* font);

    void setText(const std::string& text);
    const std::string& getText() const { return text; }
    void clear() { text.clear(); cursorIndex = 0; scrollX = 0.0f; }

    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }
    bool enabled = true;

    float getX() const { return x; }
    float getY() const { return y; }
    Vector2 getPosition() const { return {x, y}; }
    void setPosition(float nx, float ny) { x = nx; y = ny; }

    void update();
    void draw();

private:
    static constexpr float PAD = 5.0f;

    size_t indexFromLocalX(float lx) const;
    float textWidthUpTo(size_t byteIndex) const;
    float letterSpacing() const;
    Font resolveFont() const;   // custom font, or default if unset/failed to load

    float x, y, width, height;
    int fontSize;
    std::string text;
    size_t cursorIndex = 0;   // byte index into text (UTF-8 safe)
    bool active_ = false;
    float cursorTimer = 0.0f;
    float scrollX = 0.0f;      // horizontal scroll so cursor stays visible
    Color textColor = BLACK;
    Color borderColor = DARKGRAY;
    Color backgroundColor = LIGHTGRAY;
    Color cursorColor = BLACK;
    Font* font = nullptr;
    std::string prompt;
    Color promptColor = GRAY;
};
