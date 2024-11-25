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
#define MAX_FRIENDS 10  // Limite du nombre d'amis par joueur
#define MAX_PLAYER_SAVED_GAMES 50  // Limite de sauvegardes par joueur
#define MAX_CLIENTS 100  // Limite des comptes clients
#define DATA_FILE "client_data.txt"  // Fichier de sauvegarde

typedef struct {
    char opponent[BUF_SIZE];  // Pseudo de l'adversaire
    int board[12];            // Plateau final
    int scores[2];            // Scores finaux
    char result[BUF_SIZE];    // Résultat (gagné/perdu/égalité)
} PlayerSavedGame;

typedef struct {
    PlayerSavedGame saved_games[MAX_PLAYER_SAVED_GAMES];
    int saved_game_count;  // Nombre de parties sauvegardées
} PlayerHistory;


typedef struct {
    int sock;
    char pseudo[BUF_SIZE];
    int in_game; // 1 si en partie, 0 sinon
    int opponent; // Index du joueur en partie, -1 sinon
    char bio[BUF_SIZE * 10]; // Bio du joueur, 10 lignes max
    char friends[MAX_FRIENDS][BUF_SIZE]; // Liste des pseudos d'amis
    int friend_count; // Nombre d'amis
    PlayerHistory history;  // Historique des parties sauvegardées
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char pseudo[BUF_SIZE];
    char bio[BUF_SIZE * 10];
    char friends[MAX_FRIENDS][BUF_SIZE];
    int friend_count;
    PlayerSavedGame saved_games[MAX_PLAYER_SAVED_GAMES];
    int saved_game_count;
} ClientAccount;

ClientAccount accounts[MAX_CLIENTS];
int account_count = 0;

typedef struct {
    int player1_index;
    int player2_index;
    int board[12];
    int scores[2];
    int current_player;
    int observers[MAX_OBSERVERS];  // Indices des observateurs
    int observer_count;
    int is_private;
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


// Gestion des commandes client
void handle_command(int sender_index, const char *command) {
    if (strncmp(command, "/list", 5) == 0) {
        list_clients(clients[sender_index].sock);
    } else if (strncmp(command, "/games", 6) == 0) {
        char buffer[BUF_SIZE * 10];  // Pour stocker la liste des parties
        memset(buffer, 0, sizeof(buffer));

        pthread_mutex_lock(&clients_mutex);
        if (game_count == 0) {
            snprintf(buffer, sizeof(buffer), "Aucune partie en cours pour le moment.\n");
        } else {
            snprintf(buffer, sizeof(buffer), "=== Liste des parties en cours ===\n");
            for (int i = 0; i < game_count; ++i) {
                Game *game = &games[i];
                char game_info[BUF_SIZE];

                snprintf(game_info, sizeof(game_info),
                        "Partie %d : %s VS %s | Score : %d - %d \n",
                        i + 1,
                        clients[game->player1_index].pseudo,
                        clients[game->player2_index].pseudo,
                        game->scores[0],
                        game->scores[1]);
                strncat(buffer, game_info, sizeof(buffer) - strlen(buffer) - 1);
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        // Envoyer la liste des parties au joueur
        send_to_client(clients[sender_index].sock, buffer);
    } else if (strncmp(command, "/setbio ", 8) == 0) {
    pthread_mutex_lock(&clients_mutex);
    snprintf(clients[sender_index].bio, sizeof(clients[sender_index].bio), "%s", command + 8);
    send_to_client(clients[sender_index].sock, "Votre bio a été mise à jour.\n");
    pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/viewbio", 8) == 0) {
        char response[BUF_SIZE * 2]; // Buffer pour la réponse
        pthread_mutex_lock(&clients_mutex);

        // Vérifier si le client a une bio
        if (clients[sender_index].bio[0] != '\0') {
            snprintf(response, sizeof(response), "Votre bio :\n%s\n", clients[sender_index].bio);
        } else {
            snprintf(response, sizeof(response), "Vous n'avez pas encore défini de bio.\n");
        }

        pthread_mutex_unlock(&clients_mutex);
        send_to_client(clients[sender_index].sock, response);
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
    } else if (strncmp(command, "/quitgame", 9) == 0) {
        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                int winner_index = (game->player1_index == sender_index) ? game->player2_index : game->player1_index;

                // Notifier l'adversaire
                char message[BUF_SIZE];
                snprintf(message, BUF_SIZE, "Votre adversaire (%s) a quitté la partie. Vous êtes déclaré vainqueur !\n",
                        clients[sender_index].pseudo);
                send_to_client(clients[winner_index].sock, message);

                // Notifier le joueur qui quitte
                send_to_client(clients[sender_index].sock, "Vous avez quitté la partie. Votre adversaire est déclaré vainqueur.\n");

                // Notifier les observateurs
                for (int j = 0; j < game->observer_count; j++) {
                    int observer_index = game->observers[j];
                    send_to_client(clients[observer_index].sock, "La partie que vous observiez est terminée. Un joueur a quitté la partie.\n");
                }

                // Retirer la partie de la liste
                for (int j = i; j < game_count - 1; j++) {
                    games[j] = games[j + 1];
                }
                game_count--;

                pthread_mutex_unlock(&clients_mutex);
                return;
            }
        }

        send_to_client(clients[sender_index].sock, "Vous n'êtes pas en partie pour utiliser cette commande.\n");
        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/savegame", 9) == 0) {
        pthread_mutex_lock(&clients_mutex);

        // Vérifier si le joueur est dans une partie
        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                // Obtenir l'index de l'adversaire
                int opponent_index = (game->player1_index == sender_index) ? game->player2_index : game->player1_index;

                // Vérifier si l'historique est plein
                if (clients[sender_index].history.saved_game_count >= MAX_PLAYER_SAVED_GAMES) {
                    send_to_client(clients[sender_index].sock, "Vous avez atteint la limite de parties sauvegardées.\n");
                    pthread_mutex_unlock(&clients_mutex);
                    return;
                }

                // Sauvegarder la partie
                PlayerHistory *history = &clients[sender_index].history;
                PlayerSavedGame *saved_game = &history->saved_games[history->saved_game_count++];
                strcpy(saved_game->opponent, clients[opponent_index].pseudo);
                memcpy(saved_game->board, game->board, sizeof(game->board));
                saved_game->scores[0] = game->scores[0];
                saved_game->scores[1] = game->scores[1];

                // Déterminer le résultat
                if (game->scores[0] == game->scores[1]) {
                    strcpy(saved_game->result, "Égalité");
                } else if ((game->player1_index == sender_index && game->scores[0] > game->scores[1]) ||
                        (game->player2_index == sender_index && game->scores[1] > game->scores[0])) {
                    strcpy(saved_game->result, "Gagné");
                } else {
                    strcpy(saved_game->result, "Perdu");
                }

                // Informer le joueur
                send_to_client(clients[sender_index].sock, "La partie a été sauvegardée dans votre historique.\n");

                pthread_mutex_unlock(&clients_mutex);
                return;
            }
        }

        send_to_client(clients[sender_index].sock, "Vous n'êtes pas en partie pour sauvegarder.\n");
        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/mygames", 8) == 0) {
        pthread_mutex_lock(&clients_mutex);

        PlayerHistory *history = &clients[sender_index].history;

        if (history->saved_game_count == 0) {
            send_to_client(clients[sender_index].sock, "Vous n'avez aucune partie sauvegardée.\n");
        } else {
            char buffer[BUF_SIZE * 10];
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "=== Votre historique de parties sauvegardées ===\n");

            for (int i = 0; i < history->saved_game_count; ++i) {
                PlayerSavedGame *saved_game = &history->saved_games[i];
                char game_info[BUF_SIZE];
                snprintf(game_info, sizeof(game_info),
                        "Partie %d : Contre %s | Résultat : %s | Score : %d - %d\n",
                        i + 1,
                        saved_game->opponent,
                        saved_game->result,
                        saved_game->scores[0],
                        saved_game->scores[1]);
                strncat(buffer, game_info, sizeof(buffer) - strlen(buffer) - 1);
            }

            send_to_client(clients[sender_index].sock, buffer);
        }

        pthread_mutex_unlock(&clients_mutex);
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
    } else if (isdigit(command[0])) {
    int hole = atoi(command);
    process_move(sender_index, hole);
   } else if (strncmp(command, "/observe ", 9) == 0) {
        // Observer une partie
        char player1[BUF_SIZE], player2[BUF_SIZE];
        sscanf(command + 9, "%s %s", player1, player2);
        add_observer(sender_index, player1, player2);
    } else if (strncmp(command, "/quitobserve", 12) == 0) {
        pthread_mutex_lock(&clients_mutex);

        int found = 0;

        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            // Vérifier si le client est dans la liste des observateurs
            for (int j = 0; j < game->observer_count; j++) {
                if (game->observers[j] == sender_index) {
                    found = 1;

                    // Supprimer l'observateur
                    for (int k = j; k < game->observer_count - 1; k++) {
                        game->observers[k] = game->observers[k + 1];
                    }
                    game->observer_count--;

                    send_to_client(clients[sender_index].sock, "Vous avez quitté l'observation de la partie.\n");
                    break;
                }
            }

            if (found) break;
        }

        if (!found) {
            send_to_client(clients[sender_index].sock, "Vous n'observez aucune partie.\n");
        }

        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/chat ", 6) == 0) {
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
    } else if (strncmp(command, "/global ", 8) == 0) {
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
        char target_pseudo[BUF_SIZE], message[BUF_SIZE];
        memset(message, 0, BUF_SIZE);         // Réinitialisation du message
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
            memset(private_message, 0, BUF_SIZE); // Réinitialisation du buffer privé
            snprintf(private_message, BUF_SIZE, "[%s -> Vous] %s\n", clients[sender_index].pseudo, message);
            send_to_client(clients[target_index].sock, private_message);

            snprintf(private_message, BUF_SIZE, "[Vous -> %s] %s\n", clients[target_index].pseudo, message);
            send_to_client(clients[sender_index].sock, private_message);
        } else {
            send_to_client(clients[sender_index].sock, "Joueur introuvable.\n");
        }
        pthread_mutex_unlock(&clients_mutex);
    }else if (strncmp(command, "/addfriend ", 11) == 0) {
        char friend_pseudo[BUF_SIZE];
        sscanf(command + 11, "%s", friend_pseudo);

        pthread_mutex_lock(&clients_mutex);
        // Vérifier si le pseudo existe
        int friend_index = -1;
        for (int i = 0; i < client_count; ++i) {
            if (strcmp(clients[i].pseudo, friend_pseudo) == 0) {
                friend_index = i;
                break;
            }
        }

        if (friend_index != -1) {
            // Vérifier si l'ami est déjà dans la liste
            for (int j = 0; j < clients[sender_index].friend_count; ++j) {
                if (strcmp(clients[sender_index].friends[j], friend_pseudo) == 0) {
                    send_to_client(clients[sender_index].sock, "Cet utilisateur est déjà dans votre liste d'amis.\n");
                    pthread_mutex_unlock(&clients_mutex);
                    return;
                }
            }

            // Ajouter l'ami
            if (clients[sender_index].friend_count < MAX_FRIENDS) {
                strncpy(clients[sender_index].friends[clients[sender_index].friend_count++], friend_pseudo, BUF_SIZE);
                send_to_client(clients[sender_index].sock, "Utilisateur ajouté à votre liste d'amis.\n");
            } else {
                send_to_client(clients[sender_index].sock, "Votre liste d'amis est pleine.\n");
            }
        } else {
            send_to_client(clients[sender_index].sock, "Joueur introuvable.\n");
        }

        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/viewfriends", 12) == 0) {
        char response[BUF_SIZE * 2]; // Buffer pour la réponse
        pthread_mutex_lock(&clients_mutex);

        if (clients[sender_index].friend_count > 0) {
            snprintf(response, sizeof(response), "Votre liste d'amis :\n");
            for (int i = 0; i < clients[sender_index].friend_count; ++i) {
                strncat(response, clients[sender_index].friends[i], sizeof(response) - strlen(response) - 1);
                strncat(response, "\n", sizeof(response) - strlen(response) - 1);
            }
        } else {
            snprintf(response, sizeof(response), "Vous n'avez pas encore ajouté d'amis.\n");
        }

        pthread_mutex_unlock(&clients_mutex);
        send_to_client(clients[sender_index].sock, response);
    } else if (strncmp(command, "/private", 8) == 0) {
        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                game->is_private = 1; // Activer le mode privé
                char buffer[BUF_SIZE];
                snprintf(buffer, BUF_SIZE, "Mode privé activé. Seuls vos amis peuvent observer la partie.\n");
                send_to_client(clients[sender_index].sock, buffer);

                // Déconnecter les observateurs non autorisés
                for (int j = 0; j < game->observer_count; j++) {
                    int observer_index = game->observers[j];
                    int is_friend = 0;

                    // Vérifier si l'observateur est un ami
                    for (int k = 0; k < clients[game->player1_index].friend_count; k++) {
                        if (strcmp(clients[game->player1_index].friends[k], clients[observer_index].pseudo) == 0) {
                            is_friend = 1;
                            break;
                        }
                    }
                    for (int k = 0; k < clients[game->player2_index].friend_count; k++) {
                        if (strcmp(clients[game->player2_index].friends[k], clients[observer_index].pseudo) == 0) {
                            is_friend = 1;
                            break;
                        }
                    }

                    // Si l'observateur n'est pas un ami, le retirer
                    if (!is_friend) {
                        send_to_client(clients[observer_index].sock, "Vous avez été retiré de l'observation car la partie est désormais en mode privé.\n");
                        for (int k = j; k < game->observer_count - 1; k++) {
                            game->observers[k] = game->observers[k + 1];
                        }
                        game->observer_count--;
                        j--; // Réajuster l'index
                    }
                }

                pthread_mutex_unlock(&clients_mutex);
                return;
            }
        }

        send_to_client(clients[sender_index].sock, "Vous devez être en partie pour activer le mode privé.\n");
        pthread_mutex_unlock(&clients_mutex);
    }else if (strncmp(command, "/public", 7) == 0) {
        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                // Activer le mode public
                //game->observer_count = 0; // Supprimer tous les observateurs actuels
                game->is_private=0;
                send_to_client(clients[sender_index].sock, "Mode public activé. Tout le monde peut observer la partie.\n");
                pthread_mutex_unlock(&clients_mutex);
                return;
            }
        }

        pthread_mutex_unlock(&clients_mutex);
    } else if (strncmp(command, "/disconnect", 11) == 0) {
        pthread_mutex_lock(&clients_mutex);

        // Vérifier si le client est en partie
        for (int i = 0; i < game_count; ++i) {
            Game *game = &games[i];

            // Si le client est un joueur dans une partie
            if (game->player1_index == sender_index || game->player2_index == sender_index) {
                int opponent_index = (game->player1_index == sender_index) ? game->player2_index : game->player1_index;

                // Notifier l'adversaire
                char message[BUF_SIZE];
                snprintf(message, BUF_SIZE, "Votre adversaire (%s) s'est déconnecté. Vous êtes déclaré vainqueur !\n",
                        clients[sender_index].pseudo);
                send_to_client(clients[opponent_index].sock, message);

                // Notifier les observateurs
                for (int j = 0; j < game->observer_count; ++j) {
                    int observer_index = game->observers[j];
                    send_to_client(clients[observer_index].sock, "La partie que vous observiez est terminée. Un joueur s'est déconnecté.\n");
                }

                // Supprimer la partie
                for (int j = i; j < game_count - 1; j++) {
                    games[j] = games[j + 1];
                }
                game_count--;
                break;
            }

            // Si le client est un observateur
            for (int j = 0; j < game->observer_count; ++j) {
                if (game->observers[j] == sender_index) {
                    // Retirer l'observateur
                    for (int k = j; k < game->observer_count - 1; ++k) {
                        game->observers[k] = game->observers[k + 1];
                    }
                    game->observer_count--;

                    send_to_client(clients[sender_index].sock, "Vous avez quitté l'observation car vous vous êtes déconnecté.\n");
                    break;
                }
            }
        }

        // Notifier le client de sa déconnexion
        send_to_client(clients[sender_index].sock, "Vous êtes déconnecté du serveur.\n");

        // Supprimer le client de la liste
        close(clients[sender_index].sock);
        for (int i = sender_index; i < client_count - 1; ++i) {
            clients[i] = clients[i + 1];
        }
        client_count--;

        pthread_mutex_unlock(&clients_mutex);

        // Terminer la gestion de ce client
        pthread_exit(NULL);
    } else {
        // Commande non reconnue
        send_to_client(clients[sender_index].sock, "Commande inconnue.\n");
    }
}

void handle_disconnect(int client_index) {
    pthread_mutex_lock(&clients_mutex);

    ClientAccount *account = NULL;
    for (int i = 0; i < account_count; ++i) {
        if (strcmp(accounts[i].pseudo, clients[client_index].pseudo) == 0) {
            account = &accounts[i];
            break;
        }
    }

    if (account) {
        strcpy(account->bio, clients[client_index].bio);
        memcpy(&account->saved_games, &clients[client_index].history, sizeof(PlayerHistory));
        account->friend_count = clients[client_index].friend_count;
        for (int i = 0; i < account->friend_count; ++i) {
            strcpy(account->friends[i], clients[client_index].friends[i]);
        }
    }

    close(clients[client_index].sock);
    for (int i = client_index; i < client_count - 1; ++i) {
        clients[i] = clients[i + 1];
    }
    client_count--;

    pthread_mutex_unlock(&clients_mutex);

    save_accounts_to_file();
}

int find_or_create_account(const char *pseudo) {
    for (int i = 0; i < account_count; ++i) {
        if (strcmp(accounts[i].pseudo, pseudo) == 0) {
            return i;  // Compte existant
        }
    }

    // Créer un nouveau compte
    if (account_count < MAX_CLIENTS) {
        ClientAccount *new_account = &accounts[account_count++];
        strcpy(new_account->pseudo, pseudo);
        new_account->bio[0] = '\0';  // Pas de bio par défaut
        new_account->friend_count = 0;
        new_account->saved_game_count = 0;
        return account_count - 1;
    }

    return -1;  // Limite atteinte
}


void save_accounts_to_file() {
    FILE *file = fopen(DATA_FILE, "w");
    if (!file) {
        perror("Erreur d'ouverture du fichier de sauvegarde");
        return;
    }

    for (int i = 0; i < account_count; ++i) {
        ClientAccount *account = &accounts[i];
        fprintf(file, "%s\n", account->pseudo);
        fprintf(file, "%s\n", account->bio);

        // Sauvegarder les amis
        fprintf(file, "%d\n", account->friend_count);
        for (int j = 0; j < account->friend_count; ++j) {
            fprintf(file, "%s\n", account->friends[j]);
        }

        // Sauvegarder les parties
        fprintf(file, "%d\n", account->saved_game_count);
        for (int j = 0; j < account->saved_game_count; ++j) {
            PlayerSavedGame *game = &account->saved_games[j];
            fprintf(file, "%s\n", game->opponent);
            fprintf(file, "%d %d\n", game->scores[0], game->scores[1]);
            fprintf(file, "%s\n", game->result);
        }
    }

    fclose(file);
    printf("Comptes sauvegardés dans %s.\n", DATA_FILE);
}

void save_client_data(int client_index) {
    pthread_mutex_lock(&clients_mutex);

    // Trouver le compte associé au client
    for (int i = 0; i < account_count; ++i) {
        if (strcmp(accounts[i].pseudo, clients[client_index].pseudo) == 0) {
            // Mettre à jour les données du compte
            strncpy(accounts[i].bio, clients[client_index].bio, sizeof(accounts[i].bio) - 1);
            accounts[i].friend_count = clients[client_index].friend_count;
            for (int j = 0; j < accounts[i].friend_count; ++j) {
                strncpy(accounts[i].friends[j], clients[client_index].friends[j], BUF_SIZE - 1);
            }
            memcpy(&accounts[i].saved_games, &clients[client_index].history, sizeof(PlayerHistory));
            break;
        }
    }

    // Sauvegarder dans le fichier
    save_accounts_to_file();

    pthread_mutex_unlock(&clients_mutex);
}

// Gestion des clients
void *client_handler(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUF_SIZE];
    int index = -1;

    // Recevoir le pseudo
    if (read(sock, buffer, sizeof(buffer)) > 0) {
        buffer[strcspn(buffer, "\n")] = '\0';  // Supprimer le caractère '\n'

        pthread_mutex_lock(&clients_mutex);

        // Vérifier si ce pseudo est déjà utilisé dans la session active
        for (int i = 0; i < client_count; ++i) {
            if (strcmp(clients[i].pseudo, buffer) == 0) {
                send_to_client(sock, "Pseudo déjà utilisé. Veuillez en choisir un autre.\n");
                pthread_mutex_unlock(&clients_mutex);
                close(sock);
                pthread_exit(NULL);
            }
        }

        // Trouver ou créer un compte pour ce pseudo
        int account_index = find_or_create_account(buffer);
        if (account_index == -1) {
            send_to_client(sock, "Serveur plein. Impossible de créer un nouveau compte.\n");
            pthread_mutex_unlock(&clients_mutex);
            close(sock);
            pthread_exit(NULL);
        }

        // Charger les données du compte dans la structure client
        index = client_count++;
        clients[index].sock = sock;
        strncpy(clients[index].pseudo, accounts[account_index].pseudo, BUF_SIZE - 1);
        memcpy(&clients[index].history, &accounts[account_index].saved_games, sizeof(PlayerHistory));
        strncpy(clients[index].bio, accounts[account_index].bio, sizeof(clients[index].bio) - 1);
        clients[index].friend_count = accounts[account_index].friend_count;
        for (int i = 0; i < accounts[account_index].friend_count; ++i) {
            strncpy(clients[index].friends[i], accounts[account_index].friends[i], BUF_SIZE - 1);
        }

        clients[index].in_game = 0;
        clients[index].opponent = -1;

        pthread_mutex_unlock(&clients_mutex);

        // Envoyer un message de bienvenue
        char welcome_message[BUF_SIZE];
        snprintf(welcome_message, BUF_SIZE, "Bienvenue %s !\n", clients[index].pseudo);
        send_to_client(sock, welcome_message);
    }

    // Écouter les commandes du client
    while (1) {
        memset(buffer, 0, sizeof(buffer));  // Réinitialiser le buffer
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);  // Lire la commande
        if (bytes_read <= 0) break;  // Fin de connexion

        buffer[bytes_read] = '\0';  // Terminer la chaîne
        handle_command(index, buffer);  // Traiter la commande
    }

    // Déconnexion
    printf("Client déconnecté : %s\n", clients[index].pseudo);

    // Sauvegarder les données du client
    save_client_data(index);

    pthread_mutex_lock(&clients_mutex);
    clients[index] = clients[--client_count];
    pthread_mutex_unlock(&clients_mutex);

    pthread_exit(NULL);
}




void load_accounts_from_file() {
    FILE *file = fopen(DATA_FILE, "r");
    if (!file) {
        perror("Aucun fichier de sauvegarde trouvé. Création d'un nouveau fichier...");
        return;
    }

    account_count = 0;
    while (!feof(file)) {
        ClientAccount *account = &accounts[account_count];
        if (fscanf(file, "%s\n", account->pseudo) != 1) break;
        fgets(account->bio, sizeof(account->bio), file);
        account->bio[strcspn(account->bio, "\n")] = '\0'; // Supprimer le '\n'

        fscanf(file, "%d\n", &account->friend_count);
        for (int i = 0; i < account->friend_count; ++i) {
            fscanf(file, "%s\n", account->friends[i]);
        }

        fscanf(file, "%d\n", &account->saved_game_count);
        for (int i = 0; i < account->saved_game_count; ++i) {
            PlayerSavedGame *game = &account->saved_games[i];
            fscanf(file, "%s\n", game->opponent);
            fscanf(file, "%d %d\n", &game->scores[0], &game->scores[1]);
            fgets(game->result, sizeof(game->result), file);
            game->result[strcspn(game->result, "\n")] = '\0'; // Supprimer le '\n'
        }

        account_count++;
    }

    fclose(file);
    printf("Comptes chargés depuis %s.\n", DATA_FILE);
}



void initialize_game(int player1_index, int player2_index) {
    Game *game = &games[game_count++];
    game->player1_index = player1_index;
    game->player2_index = player2_index;
    game->is_private = 0;

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

            // Vérifier si la partie est en mode privé
            if (game->is_private) {
                int is_friend = 0;

                // Vérifier si l'observateur est ami avec l'un des deux joueurs
                for (int j = 0; j < clients[game->player1_index].friend_count; j++) {
                    if (strcmp(clients[game->player1_index].friends[j], clients[observer_index].pseudo) == 0) {
                        is_friend = 1;
                        break;
                    }
                }
                for (int j = 0; j < clients[game->player2_index].friend_count; j++) {
                    if (strcmp(clients[game->player2_index].friends[j], clients[observer_index].pseudo) == 0) {
                        is_friend = 1;
                        break;
                    }
                }

                // Refuser l'accès si l'observateur n'est pas un ami
                if (!is_friend) {
                    send_to_client(clients[observer_index].sock, "Vous n'êtes pas autorisé à observer cette partie. La partie est en mode privé.\n");
                    pthread_mutex_unlock(&clients_mutex);
                    return;
                }
            }

            // Ajouter l'observateur (uniquement si autorisé)
            if (game->observer_count < MAX_OBSERVERS) {
                game->observers[game->observer_count++] = observer_index;
                send_to_client(clients[observer_index].sock, "Vous observez maintenant la partie.\n");

                // Envoyer l'état initial de la partie
                send_game_state(game, game->player1_index, game->player2_index);
            } else {
                send_to_client(clients[observer_index].sock, "Le nombre maximum d'observateurs est atteint.\n");
            }

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

    // Charger les comptes depuis le fichier
    load_accounts_from_file();

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size)) >= 0) {
        pthread_create(&tid, NULL, client_handler, &client_sock);
    }

    close(server_sock);
    return 0;
}
