#include "raylib.h"
#include <raymath.h>
#include <stdio.h>

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
Vector2 ball = {100, 500};
Vector2 hole = {700, 100};
Vector2 velocity = {0, 0};
bool dragging = false;
Vector2 dragStart = { 0.0f, 0.0f };

// --- Custom Constants ---
const float SINK_DISTANCE = 30.0f; // Distance threshold for automatic sinking
const float SINK_PULL = 0.5f;      // Strength of the pull towards the center of the hole
const float BALL_RADIUS = 10.0f; // Ball radius for collision (assuming 20x20 ball texture)
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
 */
void GenerateNewHolePosition(int screenWidth, int screenHeight) {
    const float minDistance = 300.0f; // Minimum distance from the starting ball position
    const float margin = 50.0f; // Keep the hole 50px away from the edge
    
    // Starting position of the ball is (100, 500)
    Vector2 startPos = { 100, 500 }; 
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
void ResetGame(int screenWidth, int screenHeight) {
    // Move the hole to a new random location first!
    GenerateNewHolePosition(screenWidth, screenHeight); 
    
    // Reset ball position and state
    ball = (Vector2){100, 500};
    velocity = (Vector2){0, 0};
    strokes = 0;
    holeInOne = false;
    dragging = false;
    dragStart = (Vector2){ 0.0f, 0.0f };
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;
    const float FONT_SIZE_LG = 64.0f;
    const float FONT_SIZE_SM = 32.0f;
    // Power meter scale factor for resizing (2x bigger)
    const float POWER_METER_SCALE = 2.0f; 

    InitWindow(screenWidth, screenHeight, "Mini Golf (Desktop)");
    SetTargetFPS(60);
    
    // IMPORTANT: Seed the random number generator only once
    SetRandomSeed(GetTime()); 

    // --- LOAD ASSETS (Desktop Relative Paths) ---
    background    = LoadTexture("../assets/gfx/bg.png");
    ball_sprite   = LoadTexture("../assets/gfx/ball.png");
    ball_shadow   = LoadTexture("../assets/gfx/ball_shadow.png");
    hole_sprite   = LoadTexture("../assets/gfx/hole.png");
    arrow_sprite  = LoadTexture("../assets/gfx/point.png"); 
    settings_sprite = LoadTexture("../assets/gfx/settings.png"); 
    
    // Load Power Meter Assets
    power_bg      = LoadTexture("../assets/gfx/powermeter_bg.png");
    power_fg      = LoadTexture("../assets/gfx/powermeter_fg.png");
    power_overlay = LoadTexture("../assets/gfx/powermeter_overlay.png");
    
    // Load Custom Font
    gameFont      = LoadFontEx("../assets/font/rodin.otf", (int)FONT_SIZE_LG, NULL, 0);

    // Check for load errors
    if (background.id == 0 || ball_sprite.id == 0 || arrow_sprite.id == 0) {
        TraceLog(LOG_WARNING, "One or more assets failed to load! Using drawn shapes as fallback.");
    }
    if (gameFont.texture.id == 0) {
        TraceLog(LOG_WARNING, "Custom rodin.otf failed to load. Using default font.");
        gameFont = GetFontDefault();
    }
    // ----------------------------------------------------
    
    const float MAX_DRAG_DISTANCE = 200.0f; 
    const float ARROW_SCALE = 1.5f; 
    const float MAX_VELOCITY = 15.0f; 
    
    // Initial call to set the hole position when the game starts
    GenerateNewHolePosition(screenWidth, screenHeight); 

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
                (float)screenWidth / 2.0f - buttonWidth / 2.0f, 
                (float)screenHeight / 2.0f + 100.0f, 
                buttonWidth, 
                buttonHeight 
            };
            if (CheckCollisionPointRec(mouse, buttonRec)) {
                // IMPORTANT: Pass screen dimensions to ResetGame
                ResetGame(screenWidth, screenHeight); 
            }
        } else if (!holeInOne) {
            // Normal game input
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                Vector2Distance(GetMousePosition(), ball) < 20 &&
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
                    velocity = (Vector2){0, 0};
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
            else if (ball.x > screenWidth - BALL_RADIUS) { 
                velocity.x *= -0.8f; 
                ball.x = screenWidth - BALL_RADIUS; 
            }

            // Top boundary check
            if(ball.y < BALL_RADIUS) { 
                velocity.y *= -0.8f; 
                ball.y = BALL_RADIUS; 
            } 
            // Bottom boundary check
            else if (ball.y > screenHeight - BALL_RADIUS) { 
                velocity.y *= -0.8f; 
                ball.y = screenHeight - BALL_RADIUS; 
            }
        }


        // ----------------------------------------------------
        // --- DRAWING SECTION ---
        // ----------------------------------------------------
        BeginDrawing();
            
            // 1. Draw the Background and Hole
            if (background.id != 0) {
                Rectangle sourceRec = { 0.0f, 0.0f, (float)background.width, (float)background.height };
                Rectangle destRec = { 0.0f, 0.0f, (float)screenWidth, (float)screenHeight };
                DrawTexturePro(background, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, WHITE);
            } else {
                ClearBackground(GREEN);
            }
            
            if (hole_sprite.id != 0) {
                DrawTextureV(hole_sprite, GetCenteredPosition(hole, hole_sprite), WHITE);
            } else {
                DrawCircleV(hole, 15, DARKGRAY);
            }

            // 2. Draw the Ball
            if (!holeInOne) {
                // Shadow
                if (ball_shadow.id != 0) {
                    Vector2 shadowPos = GetCenteredPosition(ball, ball_shadow);
                    shadowPos.x += 2.0f; 
                    shadowPos.y += 2.0f;
                    DrawTextureV(ball_shadow, shadowPos, WHITE);
                }
                // Sprite
                if (ball_sprite.id != 0) {
                    DrawTextureV(ball_sprite, GetCenteredPosition(ball, ball_sprite), WHITE);
                } else {
                    DrawCircleV(ball, 10, WHITE);
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
                float powerRatio = fminf(dragDistance / MAX_DRAG_DISTANCE, 1.0f);

                // ARROW DRAWING (Same as before)
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
                    const int meterY = screenHeight - meterHeight - 20; // 20px margin from bottom

                    DrawTextureEx(power_bg, (Vector2){(float)meterX, (float)meterY}, 0.0f, POWER_METER_SCALE, WHITE);
                    
                    float clippedHeight = (float)power_fg.height * powerRatio;
                    float skippedHeight = (float)power_fg.height - clippedHeight;

                    Rectangle fgSource = { 0.0f, skippedHeight, (float)power_fg.width, clippedHeight };
                    
                    // Draw foreground, accounting for scale and position shift
                    Vector2 fgDrawPos = {(float)meterX, (float)meterY + (float)power_bg.height * POWER_METER_SCALE - clippedHeight * POWER_METER_SCALE};
                    DrawTexturePro(power_fg, fgSource, (Rectangle){fgDrawPos.x, fgDrawPos.y, (float)power_fg.width * POWER_METER_SCALE, clippedHeight * POWER_METER_SCALE}, (Vector2){0, 0}, 0.0f, WHITE);
                    
                    DrawTextureEx(power_overlay, (Vector2){(float)meterX, (float)meterY}, 0.0f, POWER_METER_SCALE, WHITE);

                } else {
                    // Fallback for Power Meter: Draw a simple red bar (Scaled)
                    const int meterX = 20;
                    const int meterY = screenHeight - 20 - 150 * 2; // Bottom left, scaled height
                    const int meterWidth = 20 * 2;
                    const int meterHeight = 150 * 2;
                    float powerHeight = (float)meterHeight * powerRatio;

                    DrawRectangle(meterX, meterY, meterWidth, meterHeight, GRAY); 
                    DrawRectangle(meterX, meterY + meterHeight - (int)powerHeight, meterWidth, (int)powerHeight, RED); 
                    DrawRectangleLines(meterX, meterY, meterWidth, meterHeight, BLACK); 
                }
            }

            // 5. Draw Stroke Counter (FIXED: Adjusting position to fit shadow)
            char strokeText[32];
            snprintf(strokeText, sizeof(strokeText), "STROKES: %d", strokes);
            
            Vector2 textSize = MeasureTextEx(gameFont, strokeText, FONT_SIZE_SM, 0);
            
            // Calculation for X position: 
            // (Screen Width) - (Text Width) - (Margin: 20px) - (Shadow Offset: 3.0f)
            // This ensures the shadow is fully visible and doesn't get clipped by the screen edge.
            Vector2 textPos = { 
                (float)screenWidth - textSize.x - 20.0f - SHADOW_OFFSET, 
                20.0f 
            };
            
            // Use custom text drawing for readability (Outline: DARKBLUE, Main: WHITE)
            DrawWiiSportsText(gameFont, strokeText, textPos, FONT_SIZE_SM, 0, DARKBLUE, WHITE);
            

            // 6. Draw Win message and Play Again button
            if (holeInOne) {
                // --- WIN TEXT (Updated to show final strokes) ---
                char winTextBuffer[128];
                Color outlineColor;

                if (strokes == 1) {
                    snprintf(winTextBuffer, sizeof(winTextBuffer), "HOLE-IN-ONE! (%d STROKE)", strokes);
                    outlineColor = (Color){ 255, 200, 0, 255 }; // Gold-Yellow
                } else {
                    snprintf(winTextBuffer, sizeof(winTextBuffer), "YOU SUNK IT IN %d STROKES!", strokes);
                    outlineColor = DARKBLUE; 
                }
                
                Vector2 winTextSize = MeasureTextEx(gameFont, winTextBuffer, FONT_SIZE_LG, 0);
                
                // FIX: Adjust horizontal position to account for SHADOW_OFFSET.
                // We shift the starting position left by half the offset (SHADOW_OFFSET / 2.0f) 
                // to visually center the entire text block (main text + shadow).
                Vector2 winTextPos = { 
                    (float)screenWidth / 2.0f - winTextSize.x / 2.0f - (SHADOW_OFFSET / 2.0f), 
                    (float)screenHeight / 2.0f - winTextSize.y / 2.0f
                };

                DrawWiiSportsText(gameFont, winTextBuffer, winTextPos, FONT_SIZE_LG, 0, outlineColor, WHITE);


                // --- PLAY AGAIN BUTTON (Transparent) ---
                const char *buttonText = "Play Again?";
                float buttonWidth = 200.0f;
                float buttonHeight = 50.0f;
                float buttonY = (float)screenHeight / 2.0f + 100.0f;

                Rectangle buttonRec = { 
                    (float)screenWidth / 2.0f - buttonWidth / 2.0f, 
                    buttonY, 
                    buttonWidth, 
                    buttonHeight 
                };
                
                // Use almost fully transparent black when not hovered (alpha=50)
                Color buttonColor = (Color){ 0, 0, 0, 50 }; 

                // Check hover state for a subtle change
                if (CheckCollisionPointRec(GetMousePosition(), buttonRec)) {
                    // Make it noticeably darker on hover (alpha=150)
                    buttonColor.a = 150; 
                }

                DrawRectangleRec(buttonRec, buttonColor);
                
                // Draw button text
                Vector2 btnTextSize = MeasureTextEx(gameFont, buttonText, FONT_SIZE_SM, 0);
                Vector2 btnTextPos = {
                    buttonRec.x + (buttonWidth / 2.0f) - (btnTextSize.x / 2.0f),
                    buttonRec.y + (buttonHeight / 2.0f) - (btnTextSize.y / 2.0f)
                };
                DrawTextEx(gameFont, buttonText, btnTextPos, FONT_SIZE_SM, 0, WHITE);
            }
        EndDrawing();
    }

    // --- UNLOAD ASSETS ---
    UnloadFont(gameFont);
    UnloadTexture(settings_sprite);
    UnloadTexture(power_overlay);
    UnloadTexture(power_fg);
    UnloadTexture(power_bg);
    UnloadTexture(arrow_sprite);
    UnloadTexture(hole_sprite);
    UnloadTexture(ball_shadow);
    UnloadTexture(ball_sprite);
    UnloadTexture(background);
    // ---------------------

    CloseWindow();
    return 0;
}
