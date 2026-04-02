#include "raylib.h"

#include "state.h"
#include "interface.h"

void interface_init() {
	// Αρχικοποίηση του παραθύρου
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "game_example");
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
	ClearBackground(RAYWHITE);

	// Σχεδιάζουμε το σκορ και το FPS counter
	//DrawText(TextFormat("%04i", state->score), 20, 20, 40, GRAY);
	DrawFPS(SCREEN_WIDTH - 80, 0);

	// Αν το παιχνίδι έχει τελειώσει, σχεδιάζουμε το μήνυμα για να ξαναρχίσει
	//if (!state->playing) {
	//	DrawText(
	//		"PRESS [ENTER] TO PLAY AGAIN",
	//		 GetScreenWidth() / 2 - MeasureText("PRESS [ENTER] TO PLAY AGAIN", 20) / 2,
	//}

	EndDrawing();
}
