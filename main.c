#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOTAL_HOLES 12
#define INITIAL_SEEDS 4
#define WINNING_SCORE 2

typedef struct {
    int board[TOTAL_HOLES]; // Plateau de jeu : 6 trous pour chaque joueur
    int score[2];           // Scores des deux joueurs
} AwaleGame;

// Initialisation du jeu
void initialize_game(AwaleGame *game) {
    for (int i = 0; i < TOTAL_HOLES; i++) {
        game->board[i] = INITIAL_SEEDS;
    }
    game->score[0] = 0;
    game->score[1] = 0;
}


// Sauvegarde de l'état final de la partie
void save_game(const AwaleGame *game, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier de sauvegarde");
        return;
    }

    fprintf(file, "État final du plateau de jeu :\n");
    fprintf(file, "Adversaire:\n");
    for (int i = 11; i >= 6; i--) {
        fprintf(file, "[%d] ", game->board[i]);
    }
    fprintf(file, "\nJoueur:\n");
    for (int i = 0; i < 6; i++) {
        fprintf(file, "[%d] ", game->board[i]);
    }
    fprintf(file, "\nScores finaux :\n");
    fprintf(file, "Joueur 0 : %d\n", game->score[0]);
    fprintf(file, "Joueur 1 : %d\n", game->score[1]);

    fclose(file);

    printf("La partie a été sauvegardée dans le fichier %s.\n", filename);
}

// Vérifie si un joueur peut jouer
int can_player_play(const AwaleGame *game, int player) {
    int start = player == 0 ? 0 : 6;
    int end = player == 0 ? 5 : 11;

    for (int i = start; i <= end; i++) {
        if (game->board[i] > 0) {
            return 1; // Le joueur a au moins un coup valide
        }
    }
    return 0; // Pas de graines disponibles
}

// Vérifie si le joueur nourrit l'adversaire
int nourishes_opponent(const AwaleGame *game, int player, int start_hole) {
    AwaleGame temp_game = *game;
    int seeds = temp_game.board[start_hole];
    temp_game.board[start_hole] = 0;
    int current_hole = start_hole;

    // Distribution des graines
    while (seeds > 0) {
        current_hole = (current_hole + 1) % TOTAL_HOLES;
        if (current_hole == start_hole) continue; // Sauter le trou initial
        temp_game.board[current_hole]++;
        seeds--;
    }

    // Vérifie si l'adversaire a au moins une graine
    int opponent_start = player == 0 ? 6 : 0;
    int opponent_end = player == 0 ? 11 : 5;

    for (int i = opponent_start; i <= opponent_end; i++) {
        if (temp_game.board[i] > 0) {
            return 1; // L'adversaire a des graines
        }
    }
    return 0; // L'adversaire est affamé
}

// Affichage du plateau
void print_board(const AwaleGame *game) {
    printf("Adversaire:\n");
    for (int i = 11; i >= 6; i--) {
        printf("[%d] ", game->board[i]);
    }
    printf("\n");
    printf("Joueur:\n");
    for (int i = 0; i < 6; i++) {
        printf("[%d] ", game->board[i]);
    }
    printf("\n");
    printf("Score Joueur: %d | Score Adversaire: %d\n", game->score[0], game->score[1]);
}

// Distribution des graines et gestion des captures
int play_move(AwaleGame *game, int player, int start_hole) {
    int opponent_start = player == 0 ? 6 : 0;
    int opponent_end = player == 0 ? 11 : 5;
    int seeds = game->board[start_hole];

    if (seeds == 0) {
        printf("Ce trou est vide ! Choisissez un autre trou.\n");
        return -1; // Coup invalide
    }

    game->board[start_hole] = 0;
    int current_hole = start_hole;

    // Distribution des graines
    while (seeds > 0) {
        current_hole = (current_hole + 1) % TOTAL_HOLES;
        if (current_hole == start_hole) continue; // Sauter le trou initial
        game->board[current_hole]++;
        seeds--;
    }

    // Gestion des captures
    int captured_seeds = 0;
    while (current_hole >= opponent_start && current_hole <= opponent_end &&
           (game->board[current_hole] == 2 || game->board[current_hole] == 3)) {
        captured_seeds += game->board[current_hole];
        game->board[current_hole] = 0;
        current_hole--;
    }

    game->score[player] += captured_seeds;
    return 0; // Coup valide
}

// Fonction pour obtenir une entrée valide
int get_valid_input(int player) {
    char input[10];
    int hole;

    while (1) {
        printf("Choisissez un trou (0-5 pour Joueur 0 ou 6-11 pour Joueur 1) : ");
        if (fgets(input, sizeof(input), stdin)) {
            if (sscanf(input, "%d", &hole) == 1) {
                if ((player == 0 && hole >= 0 && hole < 6) || (player == 1 && hole >= 6 && hole < 12)) {
                    return hole; // Entrée valide
                } else {
                    printf("Vous ne pouvez pas jouer dans les trous de l'adversaire !\n");
                }
            }
        }
        printf("Entrée invalide. Veuillez entrer un nombre correct.\n");
    }
}

// Fonction principale
int main() {
    AwaleGame game;
    initialize_game(&game);

    printf("Bienvenue dans le jeu d'Awalé !\n");
    print_board(&game);

    int current_player = 0;

    while (1) {
        if (game.score[0] >= WINNING_SCORE || game.score[1] >= WINNING_SCORE) {
            printf("Un joueur a capturé 25 graines ou plus. Fin de la partie !\n");
            break;
        }

        if (!can_player_play(&game, current_player)) {
            printf("Le joueur %d ne peut plus jouer. Fin de la partie !\n", current_player);
            break;
        }

        printf("Joueur %d, ", current_player);
        int start_hole = get_valid_input(current_player);

        if (!nourishes_opponent(&game, current_player, start_hole)) {
            printf("Ce coup affame l'adversaire ! Choisissez un autre trou.\n");
            continue;
        }

        if (play_move(&game, current_player, start_hole) == 0) {
            print_board(&game);
            current_player = 1 - current_player; // Changement de joueur
        }
    }

    printf("Fin de la partie ! Score final :\n");
    printf("Joueur 0: %d | Joueur 1: %d\n", game.score[0], game.score[1]);

    // Demande si l'utilisateur veut sauvegarder la partie
    char choice;
    printf("Voulez-vous sauvegarder la partie ? (y/n) : ");
    scanf(" %c", &choice);

    if (choice == 'y' || choice == 'Y') {
        save_game(&game, "awale_final_save.txt");
    } else {
        printf("La partie n'a pas été sauvegardée.\n");
    }

    return 0;
}
