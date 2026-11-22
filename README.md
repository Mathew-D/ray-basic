# ray-basic

A beginner-friendly [raylib](https://github.com/raysan5/raylib) template in
C++23 with a small set of reusable UI widgets, ported from
[rust-objects](https://github.com/Mathew-D/rust-objects).

Everything is designed around a **1080×720 "design space"**: place widgets
at fixed coordinates and the template scales/letterboxes them to whatever
size the window actually is — including HiDPI and fractional-scaling
setups. Clicks stay correct at any window size.

## Requirements

- [Nix](https://nixos.org/) with flakes enabled — that's it. The dev
  environment (compiler, raylib, `just`, `make`) comes from `flake.nix`.

## Commands

Run these from the repo root:

| Command        | What it does                          |
|----------------|---------------------------------------|
| `just`         | Lists all available recipes           |
| `just build`   | Compiles the game to `./game`         |
| `just run`     | Builds (if needed) and launches it    |
| `just clean`   | Deletes the built binary              |

No manual `nix develop` or `make` needed — the recipes handle it.

## The frame loop

Every frame follows the same shape:

```cpp
while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);                       // letterbox bars

    BeginMode2D(UiBeginDesignFrame(1080, 720));  // design space + mouse sync

    // 1. update: poll input, update state
    input.update();                              // TextInput focus/typing
    if (button.clicked()) { clicks++; }
    if (moveButton.held())  { pic.moveDown(); }  // hold-to-repeat works too

    // 2. draw: widgets render in call order (later = on top)
    title.draw();
    counter.draw();
    input.draw();
    button.draw();
    pic.draw();

    EndMode2D();
    EndDrawing();
}
```

`UiBeginDesignFrame` builds a camera that maps the 1080×720 design space
onto the real window and converts the mouse into widget coordinates —
including all the HiDPI/fractional-scaling handling. Requires
`FLAG_WINDOW_HIGHDPI` (already set in `main`).

## Widgets

All widgets live in `src/ui.h` / `src/ui.cpp`. Positions are in design
coordinates (the 1080×720 space).

### Label — text display

```cpp
Label title("Hello", 20, 20, 30);          // text, x, y, fontSize
title.withColors(WHITE, BLACK)            // text color, optional bg color
     .withBorder(GRAY, 1.0f)              // border color, thickness
     .withRound(8.0f);                     // rounded corners radius

// in the loop:
title.draw();

// other methods:
title.setText("New text");
title.setPosition(50, 60);
title.setFontSize(24);
title.setVisible(false);                  // or toggleVisibility()
```

Also: `withFont(Font*)`, `withFixedSize(w, h)`, `withAlignment(TextAlign::Center)`,
`getWidth()/getHeight()/getText()/isVisible()`.

### TextButton — button with text

```cpp
TextButton button(20, 115, 180, 50, "Click Me", BLUE, SKYBLUE, 24);
//                          x    y    w    h   text      normal  hover  fontSize
button.withRound(10.0f).withBorder(WHITE, 2.0f);

// in the loop — update phase:
if (button.clicked()) { /* released on the button */ }

// draw phase:
button.draw();

// or the combined convenience form:
if (button.click()) { /* draws AND reports the click */ }
```

`clicked()` only counts a real click: the press must start on the button
and the release must happen while still hovering it. `held()` returns
true every frame the button is held down — useful for hold-to-repeat
interactions like the "Move down" demo in `main.cpp`.

Public fields you can tweak directly: `enabled`, `visible`, `width`,
`height`, `normalColor`, `hoverColor`, `textColor`, `fontSize`.
Also: `withTextColor`, `withHoverTextColor`, `withAlignment`,
`withVerticalAlignment`, `setText`, `updatePosition(x, y, w, h)`.

### ImageButton — button from image files

```cpp
ImageButton imageButton("assets/btn.png", "assets/btn_hover.png",
                        20, 200, 200, 60);
//                 normal texture     hover texture          x   y   w   h

if (imageButton.clicked()) { /* ... */ }
imageButton.draw();
// or: if (imageButton.click()) { ... }
```

Hit-testing uses the image's **alpha channel** — transparent pixels
aren't clickable. If a file is missing, a placeholder PNG is generated
automatically (paths are relative to the working directory, so run from
the project root). `setImage(path, hoverPath)` swaps textures later;
`setBounds(x, y, w, h)` moves/resizes it.

### ImageView — display an image

```cpp
ImageView pic("assets/pic.png", 420, 180, 120, 120, true, 1.0f);
//                     path           x    y    w    h  stretch  zoom

pic.draw();
```

- `stretch = true` fills the w×h box; `false` shows the image at native
  size (times `zoom`).
- Transform helpers: `setPosition`, `setAngle` (radians), `setZoom`,
  `zoomIn/zoomOut`, `resetZoom`, `setTexture(newPath)`.
- Queries: `pos()`, `size()`, `textureSize()`, `isEmpty()`.

Missing files get the same auto-generated placeholder as `ImageButton`.

### Image collision — pixel-perfect

```cpp
ImageView pic("assets/pic.png", 400, 120, 120, 120, true, 1.0f);
ImageView target("assets/btn.png", 400, 270, 200, 60, true, 1.0f);

// in the update phase:
bool hit = CheckCollision(pic, target);          // skipPixels = 1
```

`CheckCollision` is pixel-perfect: it respects the alpha channel
(fully transparent pixels don't collide) and rotation from `setAngle`.
`skipPixels` trades precision for speed — `1` tests every pixel,
higher values sample every Nth. Both views must be in the same
coordinate space (the design space inside `BeginMode2D`).

The `main.cpp` demo uses this with a "Hold: Move down" button
(`moveButton.held()`) that slides `pic` until it hits `target`, plus a
"Reset pic" button to try again.

### TextInput — single-line text field

```cpp
TextInput input(20, 270, 360, 36, 20);           // x, y, w, h, fontSize
input.withColors(WHITE, GRAY, {30,30,30,255}, WHITE)  // text,border,bg,cursor
     .withPrompt("Type here...", GRAY);

// update phase (required — handles focus, clicks, typing, keys):
input.update();
// draw phase:
input.draw();
std::string text = input.getText();
```

Click to focus, click again to place the cursor, type to enter text.
Backspace/Delete/arrows/Home/End work; Enter or clicking outside
defocuses. `setText`/`clear`/`isActive`/`enabled` are available.
It's single-line — selection, clipboard, and multiline from the Rust
original aren't ported.

## Custom fonts

```cpp
Font uiFont = LoadFontEx("assets/font.ttf", 32, NULL, 0);  // once, after InitWindow
input.withFont(&uiFont);        // TextInput
label.withFont(&uiFont);        // Label
button.withFont(&uiFont);       // TextButton
...
UnloadFont(uiFont);             // before CloseWindow
```

`LoadFontEx`'s last two args select the charset — `NULL, 0` loads the
default ~ASCII set, so non-ASCII input shows `?`. Put `.ttf` files in
`assets/` (gitignored, like the PNGs). This raylib version's
`LoadFont(path)` takes no size — use `LoadFontEx`.

## Assets

Put real images/fonts in `assets/` (gitignored `*.png`, `*.ttf`). If a
widget references an image that doesn't exist, a colored placeholder
with a white circle is generated on first run — handy for prototyping
without art. To ship real art, drop your files in `assets/` and
reference the path; existing files are never overwritten.

## Layout tips

- Everything is positioned in the 1080×720 design space — the camera
  handles scaling, so don't compute window pixels yourself.
- `ClearBackground(BLACK)` fills the letterbox bars; pick any color.
- `UiDrawGrid(50.0f, {255,255,255,60})` draws a coordinate grid with
  axis labels over the design space (port of the Rust `draw_grid`) —
  grid coordinates match widget coordinates exactly, so a "400" label
  is where `x = 400` widgets go. `main.cpp` toggles it with **G**.
- Want a different design resolution? Change `DESIGN_W`/`DESIGN_H` in
  `main.cpp` (and `InitWindow` if you like) — widgets don't care.
