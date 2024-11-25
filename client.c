#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 1977
#define BUF_SIZE 1024

// Fonction pour gérer les messages reçus du serveur
void *receive_handler(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUF_SIZE];

    while (1) {
        int n = read(sock, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            printf("Déconnecté du serveur.\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        buffer[n] = '\0';
        printf("%s", buffer);

    }
    
}

// Fonction principale du client
int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage : %s [adresse IP] [pseudo]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sock;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;
    char buffer[BUF_SIZE];

    // Création du socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Conversion de l'adresse IP
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        return EXIT_FAILURE;
    }

    // Connexion au serveur
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connexion échouée");
        return EXIT_FAILURE;
    }

    // Envoi du pseudo au serveur
    write(sock, argv[2], strlen(argv[2]));

    // Création du thread pour gérer les messages reçus
    if (pthread_create(&recv_thread, NULL, receive_handler, &sock) != 0) {
        perror("Erreur de création de thread");
        return EXIT_FAILURE;
    }

    // Boucle principale pour envoyer des commandes au serveur
    printf("Bienvenue dans le jeu Awalé ! \n");
    printf("Commandes disponibles :\n");
    printf("/list                    : Liste des joueurs connectés\n");
    printf("/games                   : Liste des partis en cours\n");
    printf("/setbio [texte]          : Définir ou modifier votre bio (10 lignes max)\n");
    printf("/viewbio                 : Consulter votre bio\n");
    printf("/bio [pseudo]            : Afficher la bio d'un joueur\n");
    printf("/challenge [pseudo]      : Défier un joueur\n");
    printf("/quitgame                : Quitter une partie\n");
    printf("/savegame                : Sauvegarder une partie\n");
    printf("/mygames                 : Regarder ses parties\n");
    printf("/addfriend [pseudo]      : Ajouter un ami\n");
    printf("/viewfriends             : Consulter votre liste d'amis\n");
    printf("/private                 : Se mettre en mode privé\n");
    printf("/public                  : Se mettre en mode public\n");
    printf("/observe [p1] [p2]       : Observer une partie entre deux joueurs\n");
    printf("/quitobserve             : Quitter l'observation d'une partie\n");
    printf("/chat [message]          : Envoyer un message aux joueurs de votre partie\n");
    printf("/global [message]        : Envoyer un message général à tous les joueurs hors partie\n");
    printf("/msg [pseudo] [message]  : Envoyer un message privé à un joueur\n");
    printf("yes                      : Accepter un défi\n");
    printf("no                       : Refuser un défi\n");
    printf("0-5                      : Jouer un coup sur votre plateau\n");
    printf("/disconnect              : Se déconnecter du serveur\n");


    while (1) {
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Supprimer le caractère '\n'

        if (strlen(buffer) > 0) {
            write(sock, buffer, strlen(buffer));
        }
    }

    close(sock);
    return 0;
}
