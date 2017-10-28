# DomoGridPlug
DomoPlug on ESP8266 with Arduino
Programme permettant le contrôle d'un relais à distance.

# TODO :
- [ ] Ajouter la fonction WPS
- [ ] Serveur https (Impossible à l'heure actuel sur l'ESP8266)
- [ ] Ajouter un serveur websockets
- [ ] Authentification du client (https+compten client)

# État actuel d programme
Le programme permet :
- de se connecter à un accès point préenregistré dans le fichier.ino
- d'allumer et d'éteindre un relais à distance
- d'obtenir :
  * l'état de la prise
  * la consommation instantanée de la prise (uniquement pour des charges résistives)
  * de l'énergie consommée depuis le début du démarrage du programme
  * d'afficher :
   - la puissance du signal reçu par le module wifi ESP8266
   - le mot de passe de l'accès point
   - l'adresse MAC de l'accès point
   - l'adresse IP
   - un graphique de la puissance instantanée et moyenne en fonction du temps (sur 10 minutes)
 - de mettre à jour le code de la puce :
   - soit en utilisant la fonctionnalité OTA d'Arduino
   - soit en utilisant la page "mise à jour" du serveur
