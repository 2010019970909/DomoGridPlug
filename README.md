# HouseGridPlug
This program allows you to remotly control a relay.

Ce programme permet de contrôler un relais à distance.

# TODO
- [ ] Add the WPS function
~~- [ ] Use https server~~ (not possible on this chip)
- [ ] Add a websockets serveur
- [X] Add client authentification (~~https +~~ client account)

- [ ] Ajouter la fonction WPS
~~- [ ] Serveur https~~ (Impossible à l'heure actuel sur l'ESP8266)
- [ ] Ajouter un serveur websockets
- [X] Authentification du client (~~https +~~ compte client)

# State of the program now/État actuel du programme
This program allows to:
- connect the plug to an access point whose connection information has been pre-registered in the ".ino" file
- turn on and off a relay remotly
- get:
  * the state of the plug (relay "on" or "off")
  * the instantaneous power consumption of the plug (only for resistives loads)
  * the amount of energy consump from the starting of the program
  * display:
    - the strengh of the signal receive by wifi chip ESP8266
    - the passphrase of the access point 
    - the MAC adress of the access point
    - the IP adress of the access point
    - a graph of instantaneous and mean power consumption in function of the time (for a period of 10 minutes)
- update the code of the chip (the Arduino code only):
  - by using the OTA functionality of Arduino
  - by using the "update" page of the server


Le programme permet :
- de se connecter à un point d'accès dont les informations de connexion ont été préenregistré dans le fichier ".ino"
- d'allumer et d'éteindre un relais à distance
- d'obtenir :
  * l'état de la prise (relais "on" ou "off")
  * la consommation instantanée de la prise (uniquement pour des charges résistives)
  * de l'énergie consommée depuis le début du démarrage du programme
  * d'afficher :
    - la puissance du signal reçu par le module wifi ESP8266
    - le mot de passe du point d'accès
    - l'adresse MAC du point d'accès
    - l'adresse IP
    - un graphique de la puissance instantanée et moyenne en fonction du temps (sur 10 minutes)
- de mettre à jour le code de la puce (le code Arduino seulement) :
  - soit en utilisant la fonctionnalité OTA d'Arduino
  - soit en utilisant la page "mise à jour" du serveur
