#include "raylib.h"

#include "state.h"
#include "interface.h"

void interface_init() {
	// Αρχικοποίηση του παραθύρου
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "game");
	SetTargetFPS(60);
    //InitAudioDevice();

}

void interface_close() {
	//CloseAudioDevice();
	CloseWindow();
}

// Draw game (one frame)
void interface_draw_frame(State state) {
	BeginDrawing();

	// Καθαρισμός, θα τα σχεδιάσουμε όλα από την αρχή
	ClearBackground(DARKGRAY);

	// Σχεδιάζουμε το σκορ και το FPS counter
	//DrawText(TextFormat("%04i", state->score), 20, 20, 40, GRAY);
	DrawFPS(SCREEN_WIDTH - 80, 0);

    int piano_keys = 128;
    int piano_keys_height = 150;

    int piano_keys_start_y = SCREEN_HEIGHT - piano_keys_height;

    for (int k = 0 ; k < piano_keys ; k++) {

        int x = k * SCREEN_WIDTH / piano_keys;
        int piano_keys_next_x = (k + 1) * SCREEN_WIDTH / piano_keys;
        int piano_keys_width = piano_keys_next_x - x;
    

    DrawRectangle(x, piano_keys_start_y, piano_keys_width, piano_keys_height, RAYWHITE);

    DrawRectangleLines(x, piano_keys_start_y, piano_keys_width, piano_keys_height, BLACK);
    }
	// Αν το παιχνίδι έχει τελειώσει, σχεδιάζουμε το μήνυμα για να ξαναρχίσει
	//if (!state->playing) {
	//	DrawText(
	//		"PRESS [ENTER] TO PLAY AGAIN",
	//		 GetScreenWidth() / 2 - MeasureText("PRESS [ENTER] TO PLAY AGAIN", 20) / 2,
	//}

	EndDrawing();
}
