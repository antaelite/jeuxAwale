#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h> // Pour le tirage au sort

#define PORT 1977
#define MAX_CLIENTS 100
#define BUF_SIZE 1024
#define MAX_OBSERVERS 10

typedef struct {
    int sock;
    char pseudo[BUF_SIZE];
    int in_game; // 1 si en partie, 0 sinon
    int opponent; // Index du joueur en partie, -1 sinon
    char bio[BUF_SIZE * 10]; // Bio du joueur, 10 lignes max
} Client;


Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int player1_index;
    int player2_index;
    int board[12];
    int scores[2];
    int current_player;
    int observers[MAX_OBSERVERS];  // Indices des observateurs
    int observer_count;
} Game;

Game games[MAX_CLIENTS / 2];
int game_count = 0;

// Fonction pour envoyer un message à un client
void send_to_client(int sock, const char *message) {
    int length = strlen(message);  // Taille exacte du message
    if (write(sock, message, length) < 0) {
        perror("Erreur d'envoi au client");
    }
}

// Diffuser un message à tous les clients sauf l'expéditeur
void broadcast(const char *message, int sender_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].sock != sender_sock) {
            send_to_client(clients[i].sock, message);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Liste les pseudos des clients connectés
void list_clients(int sock) {
    char buffer[BUF_SIZE] = "Joueurs connectés :\n";
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; ++i) {
        strncat(buffer, clients[i].pseudo, BUF_SIZE - strlen(buffer) - 1);
        strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);
    }
    pthread_mutex_unlock(&clients_mutex);
    send_to_client(sock, buffer);
}

// Gestion des défis entre joueurs
void challenge_client(int sender_index, const char *opponent_pseudo) {
    pthread_mutex_lock(&clients_mutex);
    int opponent_index = -1;

    // Chercher l'adversaire disponible
    for (int i = 0; i < client_count; ++i) {
        if (strcmp(clients[i].pseudo, opponent_pseudo) == 0 && clients[i].in_game == 0) {
            opponent_index = i;
            break;
        }
    }

    if (opponent_index != -1) {
        char message[BUF_SIZE];
        snprintf(message, BUF_SIZE, "Défi de %s : acceptez-vous ? (yes/no)\n", clients[sender_index].pseudo);
        send_to_client(clients[opponent_index].sock, message);

        clients[sender_index].opponent = opponent_index;
        clients[opponent_index].opponent = sender_index;
    } else {
        send_to_client(clients[sender_index].sock, "Joueur introuvable ou déjà en partie.\n");
    }
    pthread_mutex_unlock(&clients_mutex);
}

void quit_game(int sender_index) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < game_count; i++) {
        Game *game = &games[i];

        if (game->player1_index == sender_index || game->player2_index == sender_index) {
            int opponent_index = (game->player1_index == sender_index) ? game->player2_index : game->player1_index;

            // Envoyer un message aux deux joueurs
            send_to_client(clients[game->player1_index].sock, "La partie a été arrêtée.\n");
            send_to_client(clients[game->player2_index].sock, "La partie a été arrêtée.\n");

            for (int j = 0; j < game->observer_count; j++) {
                send_to_client(clients[game->observers[j]].sock, "La partie a été arrêtée.\n");
            }

            // Mettre à jour les états des joueurs
            clients[game->player1_index].in_game = 0;
            clients[game->player2_index].in_game = 0;

            // Supprimer la partie
            games[i] = games[--game_count];

            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Fonction pour arrêter l'observation d'une partie
void end_observation(int observer_index) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < game_count; i++) {
        Game *game = &games[i];

        for (int j = 0; j < game->observer_count; j++) {
            if (game->observers[j] == observer_index) {
                // Retirer l'observateur
                for (int k = j; k < game->observer_count - 1; k++) {
                    game->observers[k] = game->observers[k + 1];
                }
                game->observer_count--;

                send_to_client(clients[observer_index].sock, "Vous avez arrêté d'observer la partie.\n");

                pthread_mutex_unlock(&clients_mutex);
                return;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Gestion des commandes client
void handle_command(int sender_index, const char *command) {
    if (strncmp(command, "/list", 5) == 0) {
        list_clients(clients[sender_index].sock);
    } else if (strncmp(command, "/setbio ", 8) == 0) {
    pthread_mutex_lock(&clients_mutex);
    snprintf(clients[sender_index].bio, sizeof(clients[sender_index].bio), "%s", command + 8);
    send_to_client(clients[sender_index].sock, "Votre bio a été mise à jour.\n");
    pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/bio ", 5) == 0) {
    char target_pseudo[BUF_SIZE];
    sscanf(command + 5, "%s", target_pseudo);

    pthread_mutex_lock(&clients_mutex);
    int target_index = -1;
    for (int i = 0; i < client_count; ++i) {
        if (strcmp(clients[i].pseudo, target_pseudo) == 0) {
            target_index = i;
            break;
        }
    }

    if (target_index != -1) {
        char response[BUF_SIZE * 11];
        snprintf(response, sizeof(response), "Bio de %s :\n%s\n",
                 clients[target_index].pseudo, clients[target_index].bio[0] ? clients[target_index].bio : "Aucune bio disponible.");
        send_to_client(clients[sender_index].sock, response);
    } else {
        send_to_client(clients[sender_index].sock, "Joueur introuvable.\n");
    }
    pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/challenge ", 11) == 0) {
        // Commande pour défier un autre joueur
        char opponent_pseudo[BUF_SIZE];
        sscanf(command + 11, "%s", opponent_pseudo);
        challenge_client(sender_index, opponent_pseudo);
    } else if (strncmp(command, "yes", 3) == 0) {
        // Accepter un défi
        pthread_mutex_lock(&clients_mutex);
        int opponent_index = clients[sender_index].opponent;
        if (opponent_index != -1) {
            clients[sender_index].in_game = 1;
            clients[opponent_index].in_game = 1;

            // Initialiser la partie avec un tirage au sort
            initialize_game(sender_index, opponent_index);

            // Réinitialiser les liens de défi
            clients[sender_index].opponent = -1;
            clients[opponent_index].opponent = -1;
        } else {
            send_to_client(clients[sender_index].sock, "Aucun défi en attente.\n");
        }
        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "no", 2) == 0) {
        // Refuser un défi
        pthread_mutex_lock(&clients_mutex);
        int opponent_index = clients[sender_index].opponent;
        if (opponent_index != -1) {
            send_to_client(clients[opponent_index].sock, "Défi refusé.\n");
            clients[sender_index].opponent = -1;
            clients[opponent_index].opponent = -1;
        }
        pthread_mutex_unlock(&clients_mutex);
    } 
    
    else if (strncmp(command, "/quit", 5) == 0) {
        quit_game(sender_index);
    } else if (strncmp(command, "/endobservation", 15) == 0) {
        end_observation(sender_index);
    }
    
    
    
    else if (isdigit(command[0])) {
    int hole = atoi(command);
    process_move(sender_index, hole);
   } else if (strncmp(command, "/observe ", 9) == 0) {
        // Observer une partie
        char player1[BUF_SIZE], player2[BUF_SIZE];
        sscanf(command + 9, "%s %s", player1, player2);
        add_observer(sender_index, player1, player2);
    }else if (strncmp(command, "/chat ", 6) == 0) {
        char message[BUF_SIZE];
        memset(message, 0, BUF_SIZE);  // Réinitialisation du buffer
        snprintf(message, BUF_SIZE, "[%s] %s\n", clients[sender_index].pseudo, command + 6);

        // Envoyer le message aux participants de la partie
        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];
            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                send_to_client(clients[game->player1_index].sock, message);
                send_to_client(clients[game->player2_index].sock, message);

                for (int j = 0; j < game->observer_count; ++j) {
                    send_to_client(clients[game->observers[j]].sock, message);
                }
                return;
            }
        }

        // Si pas en partie, message non diffusé
        send_to_client(clients[sender_index].sock, "Vous n'êtes pas en partie pour discuter.\n");
    }else if (strncmp(command, "/global ", 8) == 0) {
        // Message général pour les joueurs non en partie
        char message[BUF_SIZE];
        memset(message, 0, BUF_SIZE);  // Réinitialisation du buffer
        snprintf(message, BUF_SIZE, "[%s] %s\n", clients[sender_index].pseudo, command + 8);

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; ++i) {
            if (!clients[i].in_game) { // Seulement pour les joueurs hors partie
                send_to_client(clients[i].sock, message);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/msg ", 5) == 0) {
        // Message privé
        char target_pseudo[BUF_SIZE], message[BUF_SIZE], private_message[BUF_SIZE];
        memset(message, 0, BUF_SIZE);         // Réinitialisation du message
        memset(private_message, 0, BUF_SIZE); // Réinitialisation du buffer privé
        sscanf(command + 5, "%s %[^\n]", target_pseudo, message);

        pthread_mutex_lock(&clients_mutex);
        int target_index = -1;
        for (int i = 0; i < client_count; ++i) {
            if (strcmp(clients[i].pseudo, target_pseudo) == 0) {
                target_index = i;
                break;
            }
        }

        if (target_index != -1) {
            char private_message[BUF_SIZE];
            snprintf(private_message, BUF_SIZE, "[%s -> Vous] %s\n", clients[sender_index].pseudo, message);
            send_to_client(clients[target_index].sock, private_message);

            snprintf(private_message, BUF_SIZE, "[Vous -> %s] %s\n", clients[target_index].pseudo, message);
            send_to_client(clients[sender_index].sock, private_message);
        } else {
            send_to_client(clients[sender_index].sock, "Joueur introuvable.\n");
        }
        pthread_mutex_unlock(&clients_mutex);
    }else {
        // Commande non reconnue
        send_to_client(clients[sender_index].sock, "Commande inconnue.\n");
    }
}


// Gestion des clients
void *client_handler(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUF_SIZE];
    int index = -1;

    // Enregistrer le pseudo
    if (read(sock, buffer, sizeof(buffer)) > 0) {
        pthread_mutex_lock(&clients_mutex);
        index = client_count++;
        clients[index].sock = sock;
        strncpy(clients[index].pseudo, buffer, BUF_SIZE - 1);
        clients[index].in_game = 0;
        clients[index].opponent = -1;
        pthread_mutex_unlock(&clients_mutex);

        char welcome_message[BUF_SIZE];
        snprintf(welcome_message, BUF_SIZE, "Bienvenue %s !\n", clients[index].pseudo);
        send_to_client(sock, welcome_message);
    }

    // Écouter les commandes du client
    while (read(sock, buffer, sizeof(buffer)) > 0) {
        handle_command(index, buffer);
    }

    quit_game(index); // Arrêter la partie si le client se déconnecte


    // Déconnexion
    close(sock);
    pthread_mutex_lock(&clients_mutex);
    clients[index] = clients[--client_count];
    pthread_mutex_unlock(&clients_mutex);
    pthread_exit(NULL);
}



void initialize_game(int player1_index, int player2_index) {
    Game *game = &games[game_count++];
    game->player1_index = player1_index;
    game->player2_index = player2_index;

    // Tirage au sort pour décider qui commence
    srand(time(NULL));
    game->current_player = rand() % 2;

    // Initialiser le plateau
    for (int i = 0; i < 12; i++) {
        game->board[i] = 4;
    }

    game->scores[0] = 0;
    game->scores[1] = 0;

    // Envoyer l'état initial de la partie
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "La partie commence ! %s joue en premier.\n",
             clients[game->current_player == 0 ? player1_index : player2_index].pseudo);
    send_to_client(clients[player1_index].sock, buffer);
    send_to_client(clients[player2_index].sock, buffer);

    send_to_client(clients[game->current_player].sock, "C'est ton tour!\n");

    // Afficher le plateau initial
    send_game_state(game, player1_index, player2_index);
}

void send_game_state(Game *game, int player1_index, int player2_index) {
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "\n=== État du jeu ===\n");

    // Ligne supérieure : joueur 2 (adversaire)
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "%s :\n", clients[player2_index].pseudo);
    for (int i = 11; i >= 6; i--) {
        char temp[10];
        snprintf(temp, 10, "[%d] ", game->board[i]);
        strncat(buffer, temp, BUF_SIZE - strlen(buffer) - 1);
    }
    strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);

    // Ligne inférieure : joueur 1
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "%s :\n", clients[player1_index].pseudo);
    for (int i = 0; i < 6; i++) {
        char temp[10];
        snprintf(temp, 10, "[%d] ", game->board[i]);
        strncat(buffer, temp, BUF_SIZE - strlen(buffer) - 1);
    }
    strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);

    // Scores
    char scores[BUF_SIZE];
    snprintf(scores, BUF_SIZE, "Score %s : %d | Score %s : %d\n",
             clients[player1_index].pseudo, game->scores[0],
             clients[player2_index].pseudo, game->scores[1]);
    strncat(buffer, scores, BUF_SIZE - strlen(buffer) - 1);

    send_to_client(clients[player1_index].sock, buffer);
    send_to_client(clients[player2_index].sock, buffer);

    // Notifier les observateurs
    for (int i = 0; i < game->observer_count; ++i) {
        send_to_client(clients[game->observers[i]].sock, buffer);
    }
}


void process_move(int sender_index, int hole) {
    for (int i = 0; i < game_count; i++) {
        Game *game = &games[i];

        if (game->player1_index == sender_index || game->player2_index == sender_index) {
            int current_player_index = game->current_player == 0 ? game->player1_index : game->player2_index;
            int opponent_index = game->current_player == 0 ? game->player2_index : game->player1_index;

            // Vérifier si c'est le bon joueur qui joue
            if (sender_index != current_player_index) {
                send_to_client(clients[sender_index].sock, "Ce n'est pas votre tour !\n");
                return;
            }

            // Vérifier si le trou est valide
            int board_index = hole + (game->current_player * 6);
            if (hole < 0 || hole >= 6 || game->board[board_index] == 0) {
                send_to_client(clients[sender_index].sock, "Coup invalide ! Choisissez un autre trou.\n");
                return;
            }

            // Distribuer les graines
            int seeds = game->board[board_index];
            game->board[board_index] = 0;
            int current_hole = board_index;

            while (seeds > 0) {
                current_hole = (current_hole + 1) % 12;
                if (current_hole == board_index) continue; // Ne pas semer dans le trou initial
                game->board[current_hole]++;
                seeds--;
            }

            // Vérifier les captures
            int captured = 0;
            if (current_hole >= (1 - game->current_player) * 6 && current_hole < (2 - game->current_player) * 6) {
                while (game->board[current_hole] == 2 || game->board[current_hole] == 3) {
                    captured += game->board[current_hole];
                    game->board[current_hole] = 0;
                    current_hole--;
                }
            }

            game->scores[game->current_player] += captured;

            // Vérifier la famine (adversaire doit avoir au moins une graine)
            int opponent_start = (1 - game->current_player) * 6;
            int opponent_end = opponent_start + 5;
            int opponent_seeds = 0;
            for (int j = opponent_start; j <= opponent_end; j++) {
                opponent_seeds += game->board[j];
            }

            if (opponent_seeds == 0) {
                send_to_client(clients[sender_index].sock, "Coup interdit : vous affamez l'adversaire !\n");
                return;
            }

            // Vérifier la fin de la partie
            if (game->scores[0] >= 25 || game->scores[1] >= 25) {
                char buffer[BUF_SIZE];
                snprintf(buffer, BUF_SIZE, "Partie terminée ! %s gagne avec %d points.\n",
                         clients[game->scores[0] >= 25 ? game->player1_index : game->player2_index].pseudo,
                         game->scores[0] >= 25 ? game->scores[0] : game->scores[1]);
                send_to_client(clients[game->player1_index].sock, buffer);
                send_to_client(clients[game->player2_index].sock, buffer);
                return;
            }

            // Changer de joueur
            game->current_player = 1 - game->current_player;

            // Envoyer un message au prochain joueur
            send_to_client(clients[game->current_player].sock, "C'est ton tour!\n");

            // Envoyer l'état du jeu
            send_game_state(game, game->player1_index, game->player2_index);

            return;
        }
    }

    send_to_client(clients[sender_index].sock, "Vous n'êtes pas en partie.\n");
}



void add_observer(int observer_index, const char *player1, const char *player2) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < game_count; ++i) {
        Game *game = &games[i];

        if ((strcmp(clients[game->player1_index].pseudo, player1) == 0 &&
             strcmp(clients[game->player2_index].pseudo, player2) == 0) ||
            (strcmp(clients[game->player1_index].pseudo, player2) == 0 &&
             strcmp(clients[game->player2_index].pseudo, player1) == 0)) {
            // Vérifier la limite d'observateurs
            if (game->observer_count >= MAX_OBSERVERS) {
                send_to_client(clients[observer_index].sock, "Nombre maximum d'observateurs atteint pour cette partie.\n");
                pthread_mutex_unlock(&clients_mutex);
                return;
            }

            // Ajouter l'observateur
            game->observers[game->observer_count++] = observer_index;
            send_to_client(clients[observer_index].sock, "Vous observez maintenant la partie.\n");

            // Envoyer l'état initial de la partie
            send_game_state(game, game->player1_index, game->player2_index);

            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }

    send_to_client(clients[observer_index].sock, "Partie introuvable.\n");
    pthread_mutex_unlock(&clients_mutex);
}




// Main du serveur
int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t tid;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    printf("Serveur en écoute sur le port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size)) >= 0) {
        pthread_create(&tid, NULL, client_handler, &client_sock);
    }

    close(server_sock);
    return 0;
}