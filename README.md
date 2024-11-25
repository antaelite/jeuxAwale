
# **Manuel de l'Utilisateur - Application Awalé**

## **Introduction**
Cette application permet de jouer au jeu Awalé en mode client-serveur. Elle inclut des fonctionnalités pour défier d'autres joueurs, discuter, sauvegarder et observer des parties, tout en offrant une gestion persistante des données utilisateurs (bio, amis, parties sauvegardées).

---

## **1. Lancement de l'Application**

### **1.1 Lancement du serveur**
1. Compilez le serveur :
   ```bash
   gcc server.c -o server -pthread
   ```
2. Lancez le serveur en spécifiant le port :
   ```bash
   ./server <port>
   ```
   Exemple :
   ```bash
   ./server 12345
   ```
   - Le serveur commence à écouter les connexions sur le port spécifié.
   - Les comptes des utilisateurs précédemment connectés sont chargés si un fichier de persistance existe.

---

### **1.2 Lancement d’un client**
1. Compilez le client :
   ```bash
   gcc client.c -o client
   ```
2. Lancez le client en spécifiant l'adresse IP du serveur, le port, et le pseudo :
   ```bash
   ./client <adresse_ip> <port> <pseudo>
   ```
   Exemple :
   ```bash
   ./client 127.0.0.1 12345 user1
   ```
   - Si le pseudo existe déjà, les données associées (bio, amis, parties sauvegardées) sont restaurées.

---

## **2. Fonctionnalités pour le client**

Une fois connecté, l’utilisateur a accès aux commandes suivantes :

### **2.1 Gestion des parties**
- **Créer ou rejoindre une partie :**
  - Utilisez la commande `/challenge <pseudo>` pour défier un autre joueur.
    Exemple :
    ```
    /challenge user2
    ```
  - Le joueur défié peut accepter avec `yes` ou refuser avec `no`.
- **Jouer un coup :**
  - Une fois la partie commencée, entrez le numéro du trou (0-5) pour jouer.
    Exemple :
    ```
    3
    ```
- **Quitter une partie :**
  - Utilisez la commande `/quitgame` pour quitter une partie en cours.
    - Le joueur restant est déclaré vainqueur.
    - Tous les observateurs sont notifiés.

---

### **2.2 Observation des parties**
- **Observer une partie entre deux joueurs :**
  - Utilisez la commande `/observe <joueur1> <joueur2>`.
    Exemple :
    ```
    /observe user1 user2
    ```
- **Quitter l’observation :**
  - Utilisez la commande `/quitobserve` pour arrêter d’observer une partie.

---

### **2.3 Gestion des messages**
- **Messages dans une partie :**
  - Utilisez la commande `/chat <message>` pour envoyer un message à l’adversaire.
    Exemple :
    ```
    /chat Bonne chance !
    ```
- **Message général à tous les joueurs hors partie :**
  - Utilisez la commande `/global <message>`.
    Exemple :
    ```
    /global Bonjour tout le monde !
    ```
- **Message privé à un joueur spécifique :**
  - Utilisez la commande `/msg <pseudo> <message>`.
    Exemple :
    ```
    /msg user2 Salut, ça va ?
    ```

---

### **2.4 Gestion des informations utilisateur**
- **Définir ou modifier votre bio :**
  - Utilisez la commande `/setbio <texte>`.
    Exemple :
    ```
    /setbio Passionné d’Awalé !
    ```
- **Consulter votre bio :**
  - Utilisez la commande `/viewbio`.
- **Ajouter un ami :**
  - Utilisez la commande `/addfriend <pseudo>`.
    Exemple :
    ```
    /addfriend user2
    ```
- **Consulter votre liste d’amis :**
  - Utilisez la commande `/viewfriends`.

---

### **2.5 Gestion des sauvegardes**
- **Sauvegarder une partie terminée :**
  - Utilisez la commande `/savegame` après la fin d’une partie.
- **Voir vos parties sauvegardées :**
  - Utilisez la commande `/mygames` pour afficher vos parties sauvegardées.

---

### **2.6 Déconnexion**
- **Se déconnecter :**
  - Utilisez la commande `/disconnect`.
    - Votre session est sauvegardée.
    - Vous êtes déconnecté du serveur.

---

## **3. Fonctionnalités pour le serveur**

### **3.1 Fonctionnement général**
- Le serveur gère plusieurs connexions de clients simultanément.
- Il sauvegarde automatiquement les données des utilisateurs dans un fichier (`client_data.txt`) au moment de leur déconnexion.
- Les parties en cours et les données des utilisateurs sont persistantes entre les redémarrages du serveur.

---

### **3.2 Gestion des comptes utilisateurs**
- Les comptes utilisateurs (pseudo, bio, amis, parties sauvegardées) sont chargés depuis le fichier `client_data.txt` au démarrage.
- Si le fichier n'existe pas, un nouveau est créé automatiquement.

---

### **3.3 Logs du serveur**
- Toutes les connexions, déconnexions et commandes des clients sont affichées dans la console du serveur.
- Exemple de log :
  ```
  Client connecté : user1
  Client déconnecté : user2
  ```

---

## **4. Résumé des commandes**

### **Commandes pour le client**
| Commande          | Description                                      |
|--------------------|--------------------------------------------------|
| `/challenge <pseudo>` | Défier un autre joueur                      |
| `/quitgame`        | Quitter une partie en cours                     |
| `/observe <p1> <p2>` | Observer une partie entre deux joueurs        |
| `/quitobserve`     | Quitter l'observation                          |
| `/chat <message>`  | Envoyer un message à l’adversaire              |
| `/global <message>`| Envoyer un message à tous les joueurs hors partie |
| `/msg <pseudo> <message>` | Envoyer un message privé à un joueur    |
| `/setbio <texte>`  | Définir ou modifier votre bio                  |
| `/viewbio`         | Consulter votre bio                            |
| `/addfriend <pseudo>` | Ajouter un joueur comme ami                 |
| `/viewfriends`     | Consulter votre liste d'amis                   |
| `/savegame`        | Sauvegarder une partie terminée                |
| `/mygames`         | Voir vos parties sauvegardées                  |
| `/disconnect`      | Se déconnecter du serveur                      |

---

### **Commandes pour le serveur**
Le serveur fonctionne de manière autonome et ne nécessite pas de commandes spécifiques. Toutes les interactions se font via le client.

---

## **5. Fichier de sauvegarde**

Les données des utilisateurs sont stockées dans un fichier texte nommé **`client_data.txt`**, qui inclut :
- Le pseudo de chaque utilisateur.
- Sa bio.
- Sa liste d'amis.
- Ses parties sauvegardées.

---

## **6. Exemple d’utilisation**

1. Lancez le serveur :
   ```
   ./server 12345
   ```

2. Connectez un client :
   ```
   ./client 127.0.0.1 12345 user1
   ```

3. Défiez un autre joueur :
   ```
   /challenge user2
   ```

4. Jouez votre partie :
   ```
   3
   ```

5. Sauvegardez la partie après sa fin :
   ```
   /savegame
   ```

6. Consultez vos informations :
   ```
   /viewbio
   /viewfriends
   /mygames
   ```

7. Déconnectez-vous :
   ```
   /disconnect
   ```

---
