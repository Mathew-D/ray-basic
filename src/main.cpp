#include "raylib.h"
#include "ui.h"
#include <format>

// Design resolution — all widgets are placed in this space.
// The camera below scales/letterboxes it to the actual window size,
// so clicks stay correct even if the window is resized or forced fullscreen.
static const float DESIGN_W = 1080.0f;
static const float DESIGN_H = 720.0f;

int main()
{
    // FLAG_WINDOW_HIGHDPI is required: UiBeginDesignFrame expects the
    // logical/render pixel split it provides for correct mouse hit-testing.
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(DESIGN_W, DESIGN_H, "ray-basic");
    SetTargetFPS(60);

    // Custom font for the widgets — load once, pass &font to withFont().
    Font uiFont = LoadFontEx("assets/font.ttf", 32, NULL, 0);

    // --- Widgets (same API shape as the rust-objects originals) ---
    Label title("ray-basic", 20, 20, 30);
    title.withColors(WHITE, BLACK).withBorder(GRAY, 1.0f);

    Label counter("Clicks: 0", 20, 75, 20);
    counter.withColors(WHITE, BLACK).withBorder(GRAY, 1.0f);

    TextButton button(20, 115, 180, 50, "Click Me", BLUE, SKYBLUE, 24);
    button.withRound(10.0f).withBorder(WHITE, 2.0f);

    ImageView pic("assets/pic.png", 400, 120, 120, 120, true, 1.0f);

    ImageButton imageButton("assets/btn.png", "assets/btn_hover.png",
                            20, 200, 200, 60);

    TextInput input(20, 270, 360, 36, 20);
    input.withColors(WHITE, GRAY, {30, 30, 30, 255}, WHITE)
         .withPrompt("Type here...", GRAY)
         .withFont(&uiFont);

    ImageView target("assets/btn.png", 400, 270, 200, 60, true, 1.0f);

    TextButton moveButton(620, 400, 160, 40, "Hold: Move down",
                          GRAY, LIME, 20);
    TextButton resetButton(450, 400, 160, 40, "Reset pic",
                           GRAY, ORANGE, 20);

    Label collisionLabel("Collision: no", 400, 345, 20);
    collisionLabel.withColors(WHITE);

    int clicks = 0;
    int imageClicks = 0;
    bool wasColliding = false;
    bool showGrid = true;

    while (!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(BLACK); // letterbox bars

        BeginMode2D(UiBeginDesignFrame(DESIGN_W, DESIGN_H));

        // Update state before drawing so this frame's draws reflect it.
        input.update();

        if (button.clicked()) {
            clicks++;
            counter.setText(std::format("Clicks: {}", clicks + imageClicks));
        }
        if (imageButton.clicked()) {
            imageClicks++;
            counter.setText(std::format("Clicks: {}", clicks + imageClicks));
        }

        // Pixel-perfect collision check against the target image.
        bool colliding = CheckCollision(pic, target);
        if (colliding != wasColliding) {
            wasColliding = colliding;
            collisionLabel.setText(colliding ? "Collision: YES" : "Collision: no");
        }

        // Hold the button to slide the picture down; it stops on impact.
        if (moveButton.held() && !colliding) {
            pic.setY(pic.getY() + 3.0f);
        }
        if (resetButton.clicked()) {
            pic.setPosition({400.0f, 120.0f});
        }
        if (IsKeyPressed(KEY_G)) showGrid = !showGrid;

        // Widgets draw in call order — later draws appear on top.
        if (showGrid) UiDrawGrid(50.0f, {255, 255, 255, 60});
        title.draw();
        counter.draw();
        input.draw();
        button.draw();
        imageButton.draw();
        moveButton.draw();
        resetButton.draw();
        target.draw();
        pic.draw();
        collisionLabel.draw();
        EndMode2D();
        EndDrawing();
    }

    UnloadFont(uiFont);
    CloseWindow();
    return 0;
}
