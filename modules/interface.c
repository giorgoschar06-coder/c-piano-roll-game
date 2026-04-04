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

	StateInfo info = state_info(state); // Παίτνω το info του παιχνιδιού

    int piano_keys = 128;
    int piano_keys_height = 150;

    int piano_keys_start_y = SCREEN_HEIGHT - piano_keys_height;

    for (int k = 0 ; k < piano_keys ; k++) {

        int x = k * SCREEN_WIDTH / piano_keys;
        int piano_keys_next_x = (k + 1) * SCREEN_WIDTH / piano_keys;
        int piano_keys_width = piano_keys_next_x - x;
		
		DrawRectangle(x, piano_keys_start_y, piano_keys_width, piano_keys_height, RAYWHITE);
		
		DrawRectangleLines(x, piano_keys_start_y, piano_keys_width, piano_keys_height, BLACK);
		
		int key_major = k % 12;
	 
		if (key_major == 1 || key_major == 3 || key_major == 6 || key_major == 8 || key_major == 10) {
		DrawRectangle(x, piano_keys_start_y, piano_keys_width, piano_keys_height - 75, BLACK);
	
		DrawRectangleLines(x, piano_keys_start_y, piano_keys_width, piano_keys_height - 75, BLACK);
		}
	}

	double time_window = 5.0; // Χρόνος που έχουν οι νότες να εμφανιστούν στον χρήστη μέχρι να πέσουν στο πιάνο
		
	List notes_falling = state_displayed_notes(state, time_window); // Λίστα με τις νότες που θα εμφανιστούν μέχρι το time_window

	for (ListNode node = list_first(notes_falling);
		 node != LIST_EOF;
		 node = list_next(notes_falling, node)) {

			MidiEvent note = list_node_value(notes_falling, node);

			if (note->velocity == 0) {
				continue;
			}

			double time_diff = note->time - info->time;
			
			if (time_diff < -0.2) { // Για να μην ξαναεμφανιστεί η νότα όταν μπει στο loop 
				continue;
			}

			int note_key = note->key;
			int x = note_key * SCREEN_WIDTH / piano_keys;
			int note_width = (note_key + 1) * SCREEN_WIDTH / piano_keys - x;

			int y = piano_keys_start_y - (int)((time_diff / time_window) * piano_keys_start_y);

			int channel = (int)note->channel;

			switch (channel) {
				case 0:
				DrawRectangle(x, y - 30, note_width, 30, BEIGE);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 1:
				DrawRectangle(x, y - 30, note_width, 30, YELLOW);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 2:
				DrawRectangle(x, y - 30, note_width, 30, RED);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 3:
				DrawRectangle(x, y - 30, note_width, 30, ORANGE);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 4:
				DrawRectangle(x, y - 30, note_width, 30, GREEN);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 5:
				DrawRectangle(x, y - 30, note_width, 30, BLUE);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 6:
				DrawRectangle(x, y - 30, note_width, 30, VIOLET);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 7:
				DrawRectangle(x, y - 30, note_width, 30, SKYBLUE);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 8:
				DrawRectangle(x, y - 30, note_width, 30, PINK);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 9:
				DrawRectangle(x, y - 30, note_width, 30, MAROON);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				case 10:
				DrawRectangle(x, y - 30, note_width, 30, BROWN);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;

				default:
				DrawRectangle(x, y - 30, note_width, 30, LIME);
				//DrawRectangleLines(x, y - 30, note_width, 30, BLACK);
				break;
			}

		 }

	DrawRectangle(0 ,0, GetScreenWidth(), 150, DARKGRAY); // Για να κρύψω τις νότες έτσι ώστε να μην φαίνοντε πριν να αρχίσει το παιχνίδι

	// Σχεδιάζουμε το σκορ και το FPS counter
	DrawText(TextFormat("Time: %0.0f", info->time), 20, 20, 40, GOLD);
	DrawText(TextFormat("Score: %0.0f", info->score), 40, 20, 40, GOLD);
	DrawText(TextFormat("Accuracy: %0.0f", info->accuracy), 60, 20, 40, GOLD);
	DrawText(TextFormat("Level: %d", info->level), 80, 20, 40, GOLD);
	DrawFPS(SCREEN_WIDTH - 80, 0);

	// Αν το παιχνίδι έχει τελειώσει, σχεδιάζουμε το μήνυμα για να ξαναρχίσει
	if (info->game_over == true) {
		DrawText(
			"PRESS [ENTER] TO PLAY AGAIN",
			 GetScreenWidth() / 2 - MeasureText("PRESS [ENTER] TO PLAY AGAIN", 20) / 2,
			 GetScreenHeight() / 2 - 50, 20, RED
		);
	}

	EndDrawing();
}
