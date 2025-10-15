#include "raylib.h"
#include <raymath.h>
#include <stdio.h>
// Needed for fminf and other math functions, although raymath often includes them.
#include <math.h>

// --- Texture Declarations ---
Texture2D background;
Texture2D ball_sprite;
Texture2D ball_shadow;
Texture2D hole_sprite;
Texture2D arrow_sprite;
Texture2D settings_sprite;

// Power Meter Textures
Texture2D power_bg;
Texture2D power_fg;
Texture2D power_overlay;

// Font Declaration
Font gameFont;

// Game State Variables
int strokes = 0;
bool holeInOne = false;

// Game Positions (Global for easy reset)
Vector2 ball = {100.0f, 500.0f};
Vector2 hole = {700.0f, 100.0f};
Vector2 velocity = {0.0f, 0.0f};
bool dragging = false;
Vector2 dragStart = { 0.0f, 0.0f };

// --- Custom Constants ---
const float SINK_DISTANCE = 45.0f; // Distance threshold for automatic sinking
const float SINK_PULL = 0.5f;      // Strength of the pull towards the center of the hole
const float BALL_RADIUS = 30.0f; // Ball radius for collision (assuming 20x20 ball texture)
// This must be equal to the X offset used in DrawWiiSportsText for correct positioning
const float SHADOW_OFFSET = 3.0f;

// A simple utility to center the ball/hole texture on its position
Vector2 GetCenteredPosition(Vector2 position, Texture2D texture) {
    return (Vector2){
            position.x - texture.width / 2.0f,
            position.y - texture.height / 2.0f
    };
}

// Function to check if all power meter components loaded
bool IsPowerMeterReady() {
    return (power_bg.id != 0 && power_fg.id != 0 && power_overlay.id != 0);
}

// Custom function for drawing text with outline/shadow (Wii Sports style)
void DrawWiiSportsText(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color outlineColor, Color mainColor) {
    // Draw Outline/Shadow (Offset slightly down and right)
    // NOTE: SHADOW_OFFSET (3.0f) is used here
    DrawTextEx(font, text, (Vector2){ position.x + SHADOW_OFFSET, position.y + SHADOW_OFFSET }, fontSize, spacing, outlineColor);

    // Draw Main Text (White foreground)
    DrawTextEx(font, text, position, fontSize, spacing, mainColor);
}

/**
 * @brief Generates a new, random position for the hole.
 * NOTE: Uses GetScreenWidth() and GetScreenHeight() dynamically for better mobile support.
 */
void GenerateNewHolePosition(void) {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    const float minDistance = 300.0f; // Minimum distance from the starting ball position
    const float margin = 50.0f; // Keep the hole 50px away from the edge

    // Starting position of the ball is (100, 500)
    Vector2 startPos = { 100.0f, 500.0f };
    Vector2 newHolePos;

    // Loop until a valid position is found
    do {
        // Generate X between margin and screenWidth - margin
        newHolePos.x = (float)GetRandomValue((int)margin, screenWidth - (int)margin);
        // Generate Y between margin and screenHeight - margin
        newHolePos.y = (float)GetRandomValue((int)margin, screenHeight - (int)margin);

    } while (Vector2Distance(newHolePos, startPos) < minDistance);

    hole = newHolePos;
}


// Function to reset the game state
void ResetGame(void) {
    // Move the hole to a new random location first!
    GenerateNewHolePosition();

    // Reset ball position and state
    ball = (Vector2){100.0f, 500.0f};
    velocity = (Vector2){0.0f, 0.0f};
    strokes = 0;
    holeInOne = false;
    dragging = false;
    dragStart = (Vector2){ 0.0f, 0.0f };
}

int main(void)
{
    const float FONT_SIZE_LG = 64.0f;
    const float FONT_SIZE_SM = 32.0f;
    // Power meter scale factor for resizing (2x bigger)
    const float POWER_METER_SCALE = 2.0f;

    // 👑 Let's go true fullscreen on mobile
    // Request HighDPI (native resolution) and allow resizing for orientation changes.
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);

    // The WindowShouldClose logic will check for the Android back button too!
    // Initialize with 0,0 to use the full screen resolution automatically.
    InitWindow(0, 0, "Mini Golf (Mobile)");
    SetTargetFPS(60);

    // IMPORTANT: Seed the random number generator only once
    SetRandomSeed(GetTime());

    // --- LOAD ASSETS (PATHS ARE ALREADY FIXED) ---
    background    = LoadTexture("gfx/bg.png");
    ball_sprite   = LoadTexture("gfx/ball.png");
    ball_shadow   = LoadTexture("gfx/ball_shadow.png");
    hole_sprite   = LoadTexture("gfx/hole.png");
    arrow_sprite  = LoadTexture("gfx/point.png");
    settings_sprite = LoadTexture("gfx/settings.png");

    // Load Power Meter Assets
    power_bg      = LoadTexture("gfx/powermeter_bg.png");
    power_fg      = LoadTexture("gfx/powermeter_fg.png");
    power_overlay = LoadTexture("gfx/powermeter_overlay.png");

    // Load Custom Font
    gameFont      = LoadFontEx("font/rodin.otf", (int)FONT_SIZE_LG, NULL, 0);
    // ----------------------------------------------------

    // Check for load errors (These will now tell us if the new paths worked)
    if (background.id == 0 || ball_sprite.id == 0 || arrow_sprite.id == 0) {
        TraceLog(LOG_WARNING, "One or more assets failed to load with new paths! Check 'gfx/' and 'font/' directories.");
    }
    if (gameFont.texture.id == 0) {
        TraceLog(LOG_WARNING, "Custom font failed to load. Using default font.");
        gameFont = GetFontDefault();
    }
    // ----------------------------------------------------

    const float MAX_DRAG_DISTANCE = 200.0f;
    const float ARROW_SCALE = 1.5f;
    const float MAX_VELOCITY = 15.0f;

    // Initial call to set the hole position when the game starts (now dynamic)
    GenerateNewHolePosition();

    while (!WindowShouldClose())
    {
        // Check if the ball has stopped (used to determine if a new shot is allowed)
        bool ballStopped = Vector2LengthSqr(velocity) < 0.1f;

        // --- Input Handling ---
        if (holeInOne && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Check for Play Again Button Click
            Vector2 mouse = GetMousePosition();
            float buttonWidth = 200.0f;
            float buttonHeight = 50.0f;
            Rectangle buttonRec = {
                    ((float)GetScreenWidth() / 2.0f) - buttonWidth / 2.0f, // Added explicit cast and parentheses for clarity
                    ((float)GetScreenHeight() / 2.0f) + 100.0f, // Added explicit cast and parentheses for clarity
                    buttonWidth,
                    buttonHeight
            };
            if (CheckCollisionPointRec(mouse, buttonRec)) {
                // IMPORTANT: Use GetScreenWidth/Height for mobile
                ResetGame(); // Call updated ResetGame (no arguments)
            }
        } else if (!holeInOne) {
            // Normal game input
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                Vector2Distance(GetMousePosition(), ball) < (BALL_RADIUS * 1.5f) &&
                ballStopped) {
                dragging = true;
                dragStart = GetMousePosition();
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && dragging) {
                Vector2 dragEnd = GetMousePosition();

                Vector2 shootVector = Vector2Subtract(dragStart, dragEnd);
                float dragDistance = Vector2Length(shootVector);

                float powerScalar = fminf(dragDistance / MAX_DRAG_DISTANCE, 1.0f);

                velocity.x += shootVector.x * 0.15f * powerScalar;
                velocity.y += shootVector.y * 0.15f * powerScalar;

                dragging = false;
                strokes++;
            }
        }

        // --- Physics Update ---
        if (!holeInOne) {
            // 1. Gravity Well Effect (Sinking)
            float distToHole = Vector2Distance(ball, hole);
            if (distToHole < SINK_DISTANCE) {
                // Direction vector from ball to hole
                Vector2 direction = Vector2Normalize(Vector2Subtract(hole, ball));

                // Add velocity to pull the ball towards the center
                velocity = Vector2Add(velocity, Vector2Scale(direction, SINK_PULL));

                // When very close and moving slowly, snap it in (Win condition)
                if (distToHole < 5.0f && Vector2LengthSqr(velocity) < 1.0f) {
                    holeInOne = true;
                    ball = hole; // Snap to center
                    velocity = (Vector2){0.0f, 0.0f};
                }
            }

            // 2. Hard Velocity Cap
            float currentSpeed = Vector2Length(velocity);
            if (currentSpeed > MAX_VELOCITY) {
                velocity = Vector2Scale(Vector2Normalize(velocity), MAX_VELOCITY);
            }

            // 3. Movement and Friction
            ball = Vector2Add(ball, velocity);

            // Friction (slowing down)
            velocity = Vector2Scale(velocity, 0.95f);

            // 4. Boundary Bounce (Ball must stay within screen edges)
            // Left boundary check
            if(ball.x < BALL_RADIUS) {
                velocity.x *= -0.8f; // Reverse direction and dampen velocity (bounce)
                ball.x = BALL_RADIUS; // Snap position back to the edge
            }
                // Right boundary check
            else if (ball.x > GetScreenWidth() - BALL_RADIUS) {
                velocity.x *= -0.8f;
                ball.x = GetScreenWidth() - BALL_RADIUS;
            }

            // Top boundary check
            if(ball.y < BALL_RADIUS) {
                velocity.y *= -0.8f;
                ball.y = BALL_RADIUS;
            }
                // Bottom boundary check
            else if (ball.y > GetScreenHeight() - BALL_RADIUS) {
                velocity.y *= -0.8f;
                ball.y = GetScreenHeight() - BALL_RADIUS;
            }
        }


        // ----------------------------------------------------
        // --- DRAWING SECTION ---
        // ----------------------------------------------------
        BeginDrawing();

        // 1. Draw the Background and Hole
        if (background.id != 0) {
            Rectangle sourceRec = { 0.0f, 0.0f, (float)background.width, (float)background.height };
            // Use GetScreenWidth() and GetScreenHeight() for destination
            Rectangle destRec = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
            DrawTexturePro(background, sourceRec, destRec, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        } else {
            ClearBackground(GREEN);
        }

        float holeVisualScale = 3.0f;
        if (hole_sprite.id != 0) {
            Vector2 holeDrawPos = {
                hole.x - (hole_sprite.width * holeVisualScale) / 2.0f,
                hole.y - (hole_sprite.height * holeVisualScale) / 2.0f
            };
            DrawTextureEx(hole_sprite, holeDrawPos, 0.0f, holeVisualScale, WHITE);
        } else {
            DrawCircleV(hole, 40.0f, DARKGRAY);
        }

        // 2. Draw the Ball
        if (!holeInOne) {
            float ballVisualScale = 3.0f;
            // Shadow
            if (ball_shadow.id != 0) {
                 Vector2 shadowDrawPos = {
                    ball.x - (ball_shadow.width * ballVisualScale) / 2.0f + (SHADOW_OFFSET * ballVisualScale),
                    ball.y - (ball_shadow.height * ballVisualScale) / 2.0f + (SHADOW_OFFSET * ballVisualScale)
                };
                DrawTextureEx(ball_shadow, shadowDrawPos, 0.0f, ballVisualScale, WHITE);
            }
            // Sprite
            if (ball_sprite.id != 0) {
                 Vector2 ballDrawPos = {
                    ball.x - (ball_sprite.width * ballVisualScale) / 2.0f,
                    ball.y - (ball_sprite.height * ballVisualScale) / 2.0f
                };
                DrawTextureEx(ball_sprite, ballDrawPos, 0.0f, ballVisualScale, WHITE);
            } else {
                DrawCircleV(ball, BALL_RADIUS, WHITE);
            }
        }

        // 3. Draw Settings Button (Top Left)
        if (settings_sprite.id != 0) {
            // Draw 20px from top and left
            DrawTexture(settings_sprite, 20, 20, WHITE);
        } else {
            // Fallback for settings icon
            DrawRectangle(20, 20, 32, 32, GRAY);
        }


        // 4. Draw Aiming Arrow and Power Meter (Bottom Left)
        if (dragging)
        {
            Vector2 mousePos = GetMousePosition();
            Vector2 shootVector = Vector2Subtract(dragStart, mousePos);
            float dragDistance = Vector2Length(shootVector);
            // NOTE: fminf ensures the power ratio doesn't exceed 1.0f (100%)
            float powerRatio = fminf(dragDistance / MAX_DRAG_DISTANCE, 1.0f);

            // ARROW DRAWING
            if (arrow_sprite.id != 0) {
                Vector2 shotDirection = Vector2Normalize(shootVector);
                float angle = atan2f(shootVector.y, shootVector.x) * RAD2DEG + 90.0f;
                float offset = fminf(dragDistance * 0.1f + 5.0f, 40.0f);
                Vector2 arrowDrawPos = Vector2Add(ball, Vector2Scale(shotDirection, offset));

                Rectangle arrowSource = { 0.0f, 0.0f, (float)arrow_sprite.width, (float)arrow_sprite.height };
                Rectangle arrowDest = { arrowDrawPos.x, arrowDrawPos.y, (float)arrow_sprite.width * ARROW_SCALE, (float)arrow_sprite.height * ARROW_SCALE };
                Vector2 origin = { (float)arrow_sprite.width * ARROW_SCALE / 2.0f, (float)arrow_sprite.height * ARROW_SCALE / 2.0f };

                DrawTexturePro(arrow_sprite, arrowSource, arrowDest, origin, angle, WHITE);
            }

            // POWER METER DRAWING (Moved to bottom left and scaled)
            if (IsPowerMeterReady()) {
                // Calculate new meter position for bottom left, considering scale
                const int meterHeight = (int)((float)power_bg.height * POWER_METER_SCALE);
                const int meterX = 20; // 20px margin from left
                // Use GetScreenHeight() for accurate positioning on mobile
                const int meterY = GetScreenHeight() - meterHeight - 20; // 20px margin from bottom

                DrawTextureEx(power_bg, (Vector2){(float)meterX, (float)meterY}, 0.0f, POWER_METER_SCALE, WHITE);

                float clippedHeight = (float)power_fg.height * powerRatio;
                float skippedHeight = (float)power_fg.height - clippedHeight;

                Rectangle fgSource = { 0.0f, skippedHeight, (float)power_fg.width, clippedHeight };

                // Draw foreground, accounting for scale and position shift
                Vector2 fgDrawPos = {(float)meterX, (float)meterY + (float)power_bg.height * POWER_METER_SCALE - clippedHeight * POWER_METER_SCALE};
                DrawTexturePro(power_fg, fgSource, (Rectangle){fgDrawPos.x, fgDrawPos.y, (float)power_fg.width * POWER_METER_SCALE, clippedHeight * POWER_METER_SCALE}, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

                DrawTextureEx(power_overlay, (Vector2){(float)meterX, (float)meterY}, 0.0f, POWER_METER_SCALE, WHITE);

            } else {
                // Fallback for Power Meter: Draw a simple red bar (Scaled)
                const int meterX = 20;
                const int meterY = GetScreenHeight() - 20 - 150 * 2; // Bottom left, scaled height
                const int meterWidth = 20 * 2;
                const int meterHeight = 150 * 2;
                float powerHeight = (float)meterHeight * powerRatio;

                DrawRectangle(meterX, meterY, meterWidth, meterHeight, GRAY);
                DrawRectangle(meterX, meterY + meterHeight - (int)powerHeight, meterWidth, (int)powerHeight, RED);
                DrawRectangleLines(meterX, meterY, meterWidth, meterHeight, BLACK);
            }
        }

        // 5. Draw Stroke Counter
        char strokeText[32];
        snprintf(strokeText, sizeof(strokeText), "STROKES: %d", strokes);

        Vector2 textSize = MeasureTextEx(gameFont, strokeText, FONT_SIZE_SM, 0.0f);

        // Calculation for X position: (Screen Width) - (Text Width) - (Margin: 20px) - (Shadow Offset: 3.0f)
        float textX = (float)GetScreenWidth() - textSize.x - 20.0f - SHADOW_OFFSET;
        float textY = 20.0f; // 20px margin from top

        DrawWiiSportsText(gameFont, strokeText, (Vector2){textX, textY}, FONT_SIZE_SM, 0.0f, BLACK, WHITE);


        // 6. Draw Win Condition Screen
        if (holeInOne) {
            // Dim the screen slightly
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));

            const char *winText = (strokes == 1) ? "HOLE-IN-ONE!!!" : "YOU DID IT!";
            char scoreText[64];
            snprintf(scoreText, sizeof(scoreText), "Score: %d Strokes", strokes);

            // Title
            Vector2 winTextSize = MeasureTextEx(gameFont, winText, FONT_SIZE_LG, 0.0f);
            DrawWiiSportsText(gameFont, winText, (Vector2){GetScreenWidth() / 2.0f - winTextSize.x / 2.0f, GetScreenHeight() / 2.0f - 80.0f}, FONT_SIZE_LG, 0.0f, DARKGREEN, GREEN);

            // Score
            Vector2 scoreTextSize = MeasureTextEx(gameFont, scoreText, FONT_SIZE_SM, 0.0f);
            DrawWiiSportsText(gameFont, scoreText, (Vector2){GetScreenWidth() / 2.0f - scoreTextSize.x / 2.0f, GetScreenHeight() / 2.0f + 20.0f}, FONT_SIZE_SM, 0.0f, BLACK, WHITE);

            // Play Again Button
            const char *buttonText = "PLAY AGAIN";
            float buttonWidth = 200.0f;
            float buttonHeight = 50.0f;
            Rectangle buttonRec = {
                    ((float)GetScreenWidth() / 2.0f) - buttonWidth / 2.0f,
                    ((float)GetScreenHeight() / 2.0f) + 100.0f,
                    buttonWidth,
                    buttonHeight
            };

            // Check button hover/press state (optional but nice)
            Color buttonColor = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), buttonRec) ? DARKBROWN : BROWN;
            DrawRectangleRounded(buttonRec, 0.5f, 10, buttonColor);
            DrawRectangleRoundedLines(buttonRec, 0.5f, 10, BLACK);

            Vector2 btnTxtSize = MeasureTextEx(gameFont, buttonText, FONT_SIZE_SM * 0.7f, 0.0f);
            DrawTextEx(gameFont, buttonText, (Vector2){buttonRec.x + buttonRec.width / 2.0f - btnTxtSize.x / 2.0f, buttonRec.y + buttonRec.height / 2.0f - btnTxtSize.y / 2.0f}, FONT_SIZE_SM * 0.7f, 0.0f, WHITE);
        }


        EndDrawing();
        // ----------------------------------------------------
    }

    // --- UNLOAD ASSETS ---
    UnloadTexture(background);
    UnloadTexture(ball_sprite);
    UnloadTexture(ball_shadow);
    UnloadTexture(hole_sprite);
    UnloadTexture(arrow_sprite);
    UnloadTexture(settings_sprite);
    UnloadTexture(power_bg);
    UnloadTexture(power_fg);
    UnloadTexture(power_overlay);
    UnloadFont(gameFont);

    CloseWindow();

    return 0;
}
