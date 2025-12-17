#include "ALL.H"
#include "directinput_scancodes.h"

int INTERNAL_LEVEL_COUNT = 55; // Vizittel egyutt

int getstatesoundon(void) { return State->sound_on; }

// Van ugyanilyen neven topol.cpp-ben is ket fv.:
static void titkosread(void* mut, int hossz, FILE* h, const char* nev) {
    if (fread(mut, 1, hossz, h) != hossz) {
        uzenet("Corrupt file, please delete it!", nev);
    }
    unsigned char* pc = (unsigned char*)mut;
    short a = 23, b = 9782, c = 3391;

    for (int i = 0; i < hossz; i++) {
        pc[i] ^= a;
        a %= c;
        b += a * c;
        a = 31 * b + c;
    }
}

static void titkoswrite(void* mut, int hossz, FILE* h) {
    unsigned char* pc = (unsigned char*)mut;
    short a = 23, b = 9782, c = 3391;

    for (int i = 0; i < hossz; i++) {
        pc[i] ^= a;
        a %= c;
        b += a * c;
        a = 31 * b + c;
    }
    if (fwrite(mut, 1, hossz, h) != hossz) {
        hiba("Nem ment iras state file-ba!");
    }
    a = 23;
    b = 9782;
    c = 3391;

    for (int i = 0; i < hossz; i++) {
        pc[i] ^= a;
        a %= c;
        b += a * c;
        a = 31 * b + c;
    }
}

static int Checknumber_state_s = 123432112;
static int Checknumber_state_r = 123432221;

static char Statenev[] = "state.dat";

state::state(const char* filename) {
    // Beallitjuk default ertekeket:
    memset(toptens, 0, sizeof(toptens));
    memset(players, 0, sizeof(players));
    player_count = 0;
    player1[0] = 0;
    player2[0] = 0;
    sound_on = 1;
    compatibility_mode = 0; // ha igaz, kompatibilis mod
    single = 1;
    flag_tag = 0;
    player1_bike1 = 1;
    high_quality = 1;

    animated_objects = 1;
    animated_menus = 1;

    editor_filename[0] = 0;
    external_filename[0] = 0;

    reset_keys();

    // Beolvasas ha kell:
    if (!filename) {
        filename = Statenev;
    }

    if (access(filename, 0) != 0) {
        // Nem letezik state.dat:
        save();
        return;
    }

    // Itt hagyni kell fopen-t!!!!!!!!!!!!!!!!!!!!:
    FILE* h = fopen(filename, "rb");
    if (!h) {
        hiba("Nem nyilik state file!: ", filename);
    }

    int verzio = 0;
    titkosread(&verzio, 4, h, filename);
    if (verzio != 200) {
        uzenet("File version is incorrect!", "Please rename it!", filename);
    }

    titkosread(toptens, sizeof(toptens), h, filename);
    titkosread(players, sizeof(players), h, filename);
    titkosread(&player_count, sizeof(int), h, filename);
    titkosread(player1, sizeof(player1), h, filename);
    titkosread(player2, sizeof(player2), h, filename);

    titkosread(&sound_on, 4, h, filename);
    titkosread(&compatibility_mode, 4, h, filename);

    titkosread(&single, sizeof(int), h, filename);
    titkosread(&flag_tag, sizeof(int), h, filename);
    titkosread(&player1_bike1, sizeof(int), h, filename);
    titkosread(&high_quality, sizeof(int), h, filename);

    titkosread(&animated_objects, sizeof(int), h, filename);
    titkosread(&animated_menus, sizeof(int), h, filename);

    titkosread(&keys1, sizeof(keys1), h, filename);
    titkosread(&keys2, sizeof(keys2), h, filename);
    titkosread(&key_increase_screen_size, sizeof(key_increase_screen_size), h, filename);
    titkosread(&key_decrease_screen_size, sizeof(key_decrease_screen_size), h, filename);
    titkosread(&key_screenshot, sizeof(key_decrease_screen_size), h, filename);

    titkosread(&editor_filename[0], 20, h, filename);
    titkosread(&external_filename[0], 20, h, filename);

    // Magic number:
    int checknumber = 0;
    if (fread(&checknumber, 1, sizeof(checknumber), h) != sizeof(checknumber)) {
        uzenet("Corrupt file, please rename it!", filename);
    }
    if (checknumber != Checknumber_state_s && checknumber != Checknumber_state_r) {
        uzenet("Corrupt file, please rename it!", filename);
    }

    fclose(h);
}

void state::reload_toptens(void) {
    char* nev = Statenev;

    // if( access( nev, 0 ) != 0 )
    //	return;

    // Itt hagyni kell fopen-t!!!!!!!!!!!!!!!!!!!!:
    FILE* h = fopen(nev, "rb");
    while (!h) {
        dialog320("The state.dat file cannot be opened!",
                  "Please make sure state.dat is accessible,", "then press enter!");
        h = fopen(nev, "rb");
    }

    int verzio = 0;
    titkosread(&verzio, 4, h, nev);
    if (verzio != 200) {
        uzenet("File version is incorrect!", "Please rename it!", nev);
    }

    titkosread(toptens, sizeof(toptens), h, nev);

    fclose(h);
}

state::~state(void) {}

void state::save(void) {
    // Itt hagyni kell fopen-t!!!!!!!!!!!!!!!!!!!!:
    FILE* h = fopen(Statenev, "wb");
    if (!h) {
        uzenet("Could not open for write file!: ", Statenev);
    }

    int verzio = 200;
    titkoswrite(&verzio, 4, h);
    titkoswrite(toptens, sizeof(toptens), h);
    titkoswrite(players, sizeof(players), h);
    titkoswrite(&player_count, sizeof(int), h);
    titkoswrite(player1, sizeof(player1), h);
    titkoswrite(player2, sizeof(player2), h);

    titkoswrite(&sound_on, 4, h);
    titkoswrite(&compatibility_mode, 4, h);

    titkoswrite(&single, sizeof(int), h);
    titkoswrite(&flag_tag, sizeof(int), h);
    titkoswrite(&player1_bike1, sizeof(int), h);
    titkoswrite(&high_quality, sizeof(int), h);

    titkoswrite(&animated_objects, sizeof(int), h);
    titkoswrite(&animated_menus, sizeof(int), h);

    titkoswrite(&keys1, sizeof(keys1), h);
    titkoswrite(&keys2, sizeof(keys2), h);
    titkoswrite(&key_increase_screen_size, sizeof(key_increase_screen_size), h);
    titkoswrite(&key_decrease_screen_size, sizeof(key_decrease_screen_size), h);
    titkoswrite(&key_screenshot, sizeof(key_decrease_screen_size), h);

    titkoswrite(&editor_filename[0], 20, h);
    titkoswrite(&external_filename[0], 20, h);

    int checknumber = Checknumber_state_r;
    if (fwrite(&checknumber, 1, sizeof(checknumber), h) != sizeof(checknumber)) {
        hiba("Nem megy beolvasas state file-ba!: ", Statenev);
    }

    fclose(h);
}

static void exportegylevel(FILE* h, topten* pidok, int single) {
    for (int i = 0; i < pidok->times_count; i++) {
        char idostr[40];
        centiseconds_to_string(pidok->times[i], idostr, true);
        fprintf(h, "    ");
        fprintf(h, idostr);
        for (int k = 0; k < (12 - strlen(idostr)); k++) {
            fprintf(h, " ");
        }
        fprintf(h, pidok->names1[i]);
        if (!single) {
            fprintf(h, ", %s", pidok->names2[i]);
        }
        fprintf(h, "\n");
    }
}

// Ha nincs egy palyan ido, tiz percet szamol helyette:
// Ez itt szazadmasodpercben van megadva:
static int const Nincsmegpalyaido = 100 * 60 * 10;

// Anonymous total times:
void state::write_stats_anonymous_total_time(FILE* h, int single, const char* text1,
                                             const char* text2, const char* text3) {
    int sum = 0;
    for (int i = 0; i < INTERNAL_LEVEL_COUNT - 1; i++) { // ucso palyat nem szamitjuk
        int levelsum = 100000000;
        // Single:
        topten* pidok = &toptens[i].single;
        if (pidok->times_count > 0) {
            levelsum = pidok->times[0];
        }
        if (!single) {
            // Multiplayer:
            topten* pidok = &toptens[i].multi;
            if (pidok->times_count > 0 && levelsum > pidok->times[0]) {
                levelsum = pidok->times[0];
            }
        }
        if (levelsum >= Nincsmegpalyaido) {
            sum += Nincsmegpalyaido;
        } else {
            sum += levelsum;
        }
    }

    fprintf(h, "%s\n%s\n%s\n", text1, text2, text3);

    char idostr[40];
    centiseconds_to_string(sum, idostr, true);
    fprintf(h, idostr);
    fprintf(h, "\n\n");
}

void state::write_stats_player_total_time(FILE* h, const char* player_name, int single) {
    int sum = 0;
    for (int i = 0; i < INTERNAL_LEVEL_COUNT - 1; i++) { // ucso palyat nem szamitjuk
        int levelsum = 100000000;

        // Single times:
        topten* pidok = &toptens[i].single;
        for (int j = 0; j < pidok->times_count; j++) {
            if( strcmp( player_name, pidok->names1[j] ) == 0 /*||
				(!single && strcmp( player_name, pidok->names2[j] ) == 0)*/ ) {
                // Megvan nev:
                levelsum = pidok->times[j];
                break;
            }
        }

        if (!single) {
            // Multi player times:
            topten* pidok = &toptens[i].multi;
            for (int j = 0; j < pidok->times_count; j++) {
                if (strcmp(player_name, pidok->names1[j]) == 0 ||
                    strcmp(player_name, pidok->names2[j]) == 0) {
                    // Megvan nev:
                    if (levelsum > pidok->times[j]) {
                        levelsum = pidok->times[j];
                    }
                    break;
                }
            }
        }

        if (levelsum >= Nincsmegpalyaido) {
            sum += Nincsmegpalyaido;
        } else {
            sum += levelsum;
        }
    }
    char idostr[40];
    centiseconds_to_string(sum, idostr, true);
    fprintf(h, idostr);
    for (int k = 0; k < (12 - strlen(idostr)); k++) {
        fprintf(h, " ");
    }
    fprintf(h, "%s\n", player_name);
}

void state::write_stats(void) {
    FILE* h = fopen("stats.txt", "wt");
    if (!h) {
        uzenet("Could not open STATS.TXT for writing!");
    }

    fprintf(h, "This text file is generated automatically each time you quit the\n");
    fprintf(h, "ELMA.EXE program. If you modify this file, you will loose the\n");
    fprintf(h, "changes next time you run the game. This is only an output file, the\n");
    fprintf(h, "best times are stored in the STATE.DAT binary file.\n");
    fprintf(h, "Registered version 1.0\n");

    fprintf(h, "\n");

    // Single palyak:
    fprintf(h, "Single player times:\n");
    fprintf(h, "\n");
    for (int i = 0; i < INTERNAL_LEVEL_COUNT - 1; i++) { // utolsot nem irjuk ki
        fprintf(h, "Level %d, %s:\n", i + 1, getleveldescription(i));
        topten* pidok = &toptens[i].single;
        exportegylevel(h, pidok, 1);
        fprintf(h, "\n");
    }

    fprintf(h, "\n");
    // Multiplayer palyak:
    fprintf(h, "Multiplayer times:\n");
    fprintf(h, "\n");
    for (int i = 0; i < INTERNAL_LEVEL_COUNT - 1; i++) { // utolsot nem irjuk ki
        fprintf(h, "Level %d, %s:\n", i + 1, getleveldescription(i));
        topten* pidok = &toptens[i].multi;
        exportegylevel(h, pidok, 0);
        fprintf(h, "\n");
    }

    // Single player total times:
    fprintf(h, "The following are the single player total times for individual players.\n");
    fprintf(h, "If a player doesn't have a time in the top ten for a level, this\n");
    fprintf(h, "will add ten minutes to the total time.\n");

    for (int i = 0; i < player_count; i++) {
        write_stats_player_total_time(h, State->players[i].name, 1);
    }
    fprintf(h, "\n");

    // Multi player total times:
    fprintf(h, "The following are the combined total times for individual players. For each\n");
    fprintf(h, "level the best time is choosen of either the player's single player best\n");
    fprintf(h, "time, or the best multiplayer time where the player was one of the two\n");
    fprintf(h, "players.\n");
    fprintf(h, "If a player doesn't have such a time for a level, this will add ten\n");
    fprintf(h, "minutes to the total time.\n");

    for (int i = 0; i < player_count; i++) {
        write_stats_player_total_time(h, State->players[i].name, 0);
    }
    fprintf(h, "\n");

    // Anonymous total times:
    write_stats_anonymous_total_time(
        h, 1, "The following is the anonymous total time of the best single player",
        "times. If there is no single player time for a level, this will",
        "add ten minutes to the total time.");

    write_stats_anonymous_total_time(
        h, 0, "The following is the anonymous combined total time of the best",
        "single or multiplayer times. If there is no single or multiplayer",
        "time for a level, this will add ten minutes to the total time.");

    fclose(h);
}

void state::reset_keys(void) {
    keys1.gas = DIK_UP;
    keys1.brake = DIK_DOWN;
    keys1.right_volt = DIK_RIGHT;
    keys1.left_volt = DIK_LEFT;
    keys1.turn = DIK_SPACE;
    keys1.toggle_minimap = DIK_V;
    keys1.toggle_timer = DIK_T;
    keys1.toggle_visibility = DIK_1;

    keys2.gas = DIK_NUMPAD5;
    keys2.brake = DIK_NUMPAD2;
    keys2.right_volt = DIK_NUMPAD3;
    keys2.left_volt = DIK_NUMPAD1;
    keys2.turn = DIK_NUMPAD0;
    keys2.toggle_minimap = DIK_B;
    keys2.toggle_timer = DIK_Y;
    keys2.toggle_visibility = DIK_2;

    // Kozos:
    key_increase_screen_size = DIK_EQUALS;
    key_decrease_screen_size = DIK_MINUS;
    key_screenshot = DIK_I;
}

int get_player_index(const char* player_name) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player* pjatekos = &State->players[i];
        if (strcmp(pjatekos->name, player_name) == 0) {
            return i;
        }
    }
    hiba("get_player_index-ben nem talalja nevet!");
    return 0;
}
