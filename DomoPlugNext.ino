/*
   Code realise par Vincent STRAGIER pour le projet DomoGrid (2017)

   Le but de ce code est de controler une prise relais 230V (16A) a l'aide d'un ESP8266 ESP-12
   On retourne aussi l'etat du relais et de la prise (on ou off, periode d'activite du relais, puissance consommee)
   Ici, on utilise le capteur AC1015 pour effectuer la mesure du courant.

   Cette prise ne permet pas de mesurer precissement la puissance consommee.
   Effectivement, on ne mesure pas la tension et on ne peut donc pas avoir le cos(Phi) ou la valeur exacte de la tension
   La tension n'est pas mesuree, car il n'y a qu'une seule entree ADC sur l'ESP-12 pour mesurer la valeur d'une tension analogique.

   TODO:
   Ajouter du WPS
   Ajouter le mesh
   Ajouter de l'https
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266SSDP.h>
#include <ESP8266LLMNR.h>
#include <ESP8266NetBIOS.h>

#define isSecure 1
#define n 600
#define httpAuth 1

// Frequence de rafraichissement du calcul des valeurs extremes de la tension
#define frq 49

// Tension efficace du reseau electrique
#define tension 230

// Coefficient propre au capteur d'intensite (rapport entre la tension mesuree et l'intensite supposee)
#define coefi 0.01468428781204111600587371512482

// Broche a laquelle la commande du relais est connectee
uint8_t broche_relais = 0;

// Chemin depuis la racine vers l'emplacement virtuel du firmware
const char* update_path = "/firmware";

// Tableau des puissances mesurées en W
float puissances[n];

// Objet ESP8266WebServer
ESP8266WebServer serveur(80);

// Objet ESP8266HTTPUpdateServer
ESP8266HTTPUpdateServer httpUpdater;

// Puissance en Watts
float p = 0;

class DomoPlugConfig { //Objet DomoPlugConfig
  private:
    char* ssid = NULL;
    char* mdp = NULL;
    char* domaine = NULL;
    char* utilisateur = NULL;
    char* motdepasse = NULL;
    char* motdepasse_OTA = NULL;
    uint8_t broche_relais = NULL;
    short conf = 0;

  public:
    DomoPlugConfig()  {};
    ~DomoPlugConfig() {};

    char* getSsid()  {
      return ssid;
    };
    void setSsid(char* inSsid)  {
      ssid = inSsid;
    };

    char* getMdp() const {
      return mdp;
    };
    void setMdp(char* inMdp)  {
      mdp = inMdp;
    };

    char* getDomaine() const {
      return domaine;
    };
    void setDomaine(char* inDomaine)  {
      domaine = inDomaine;
    };

    char* getUtilisateur()  const {
      return utilisateur;
    };
    void setUtilisateur(char* inUtilisateur)  {
      utilisateur = inUtilisateur;
    };

    char* getMotdepasse()  const {
      return motdepasse;
    };
    void setMotdepasse(char* inMotdepasse)  {
      motdepasse = inMotdepasse;
    };

    char* getMotdepasse_OTA()  const {
      return motdepasse_OTA;
    };
    void setMotdepasse_OTA(char* inMotdepasse_OTA)  {
      motdepasse_OTA = inMotdepasse_OTA;
    };

    uint8_t getBroche_relais()  const {
      return broche_relais;
    };
    void setBroche_relais(uint8_t inBroche_relais)  {
      broche_relais = inBroche_relais;
    };

    void config(char* _ssid, char* _mdp, char* _domaine, char* _utilisateur, char* _motdepasse, char* _motdepasse_OTA, uint8_t _broche_relais) {
      ssid = _ssid;
      mdp = _mdp;
      domaine = _domaine;
      utilisateur = _utilisateur;
      motdepasse = _motdepasse;
      motdepasse_OTA = _motdepasse_OTA;
      broche_relais = _broche_relais;
      pinMode(broche_relais, OUTPUT);
      digitalWrite(broche_relais, 0);
      pinMode(LED_BUILTIN, OUTPUT);
    };

    ///Verifie la disponibilite d'un access point et se connect a l'access point precise plus haut
    bool connectMyWifi()  {
      while (WiFi.status() == WL_NO_SSID_AVAIL) {
        Serial.print(":");
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(250);
      }
      // while(WiFi.status()!=WL_CONNECTED)
      WiFi.begin(ssid, mdp);
      while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_NO_SSID_AVAIL) {
        Serial.print(".");
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
      }
      digitalWrite(LED_BUILTIN, 0);
    }
};

DomoPlugConfig prise; // Creation d'un objet DomoPlugConfig

void setup() {
  Serial.println("Demarrage");
  for (int i(0); i < n; i++) puissances[i] = 0;
  prise.config("DomoGrid", "password", "domogrid_v3", "Vincent", "domogridpass", "accuratepassword", 0);

  Serial.begin(115200);

  //if(conf.isConfig()==0)

  // La DEL integree clignote tant que la connexion n'est pas etablie
  if (WiFi.status() != WL_CONNECTED)    prise.connectMyWifi();

  Serial.print("\n Adresse IP : ");
  // Retourne l'adresse IP du module
  Serial.println(WiFi.localIP());

  // Definission du nom de domaine a diffuser sur le reseau
  if (!MDNS.begin(prise.getDomaine())) {
    Serial.println("Erreur de configuration du repondeur mDNS!");
    for (int i(0); i <= 40; i++)  {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(125);
    }
    ESP.restart();
  }

  LLMNR.begin(prise.getDomaine());
  NBNS.begin(prise.getDomaine());
  Serial.println("Repondeur mDNS demarre");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(prise.getDomaine());
  
  // Define hostname of the chip
  WiFi.hostname(prise.getDomaine())

  // No authentication by default
  ArduinoOTA.setPassword(prise.getMotdepasse_OTA());

  ArduinoOTA.onStart([]() {
    Serial.println("Demarrage");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nFin");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progression : %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Echec de l'authentification");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Echec de l'initialisation");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Echec de la connexion");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Echec de la reception");
    else if (error == OTA_END_ERROR) Serial.println("Echec de la finalisation");
  });
  ArduinoOTA.begin();
  Serial.println("Pret");

  /// Definition des urls possibles
  // Obtention de l'etat
  serveur.on("/", []() { // etat du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getPageEtat(etat()));
  });

  serveur.on("/etat", []() { // etat du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getPageEtat(etat()));
  });

  serveur.on("/p", []() { // puissance actuelle en W
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getPagePuissance(p));
  });

  serveur.on("/e", []() { // energie consommee en J
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getPageEnergie(energie(p, 0)));
  });

  serveur.on("/t", []() { // periode d'activite du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send ( 200, "text/html", getPageTempsActif(tempsActif(etat(), 0)));
  });

  serveur.on("/temps", []() { // periode d'activite du relais
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    serveur.send ( 200, "text/html",   getPageTempsActifddhhmmss(tempsActif(etat(), 0)));
  });

  serveur.on("/graph", []() { // Affichage graphique
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getPageGraph());
  });

  serveur.on("/data", []() { // Affichage graphique
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send (200, "text/html", getData(tempsActif(etat(), 0), p, energie(p, 0), etat()));
  });

//  serveur.on("/newgraph", []() { // Affichage graphique
//    #ifdef httpAuth
//    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
//      return serveur.requestAuthentication();
//    #endif
//    serveur.send (200, "text/html", getActualGraph());
//  });

  serveur.on("/rssi", []() { // puissance du signal en dBm
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send ( 200, "text/html", getPageRssi(WiFi.RSSI()));
  });

  serveur.on("/ip", []() { // adresse IP
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/plain", (String) WiFi.localIP()[0] + "." + (String) WiFi.localIP()[1] + "." + (String) WiFi.localIP()[2] + "." + (String) WiFi.localIP()[3]);
  });

  serveur.on("/mac", []() { // adresse MAC
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/plain", (String)  WiFi.macAddress());
  });

  serveur.on("/psk", []() { // mot de passe prerartage avec l'access point
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/plain", WiFi.psk());
  });

  serveur.on("/bssid", []() { // adresse MAC du point d'acces
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/plain", WiFi.BSSIDstr());
  });

  serveur.on("/ssid", []() { // adresse MAC du point d'acces
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/plain", WiFi.SSID());
  });

  serveur.on("/domaine", []() { // adresse MAC du point d'acces
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send ( 200, "text/html", getPageDomaine(prise.getDomaine(), (String) WiFi.localIP()[0] + "." + (String) WiFi.localIP()[1] + "." + (String) WiFi.localIP()[2] + "." + (String) WiFi.localIP()[3]));
  });

  serveur.on("/restart", []() { // Redemarrage de la carte
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    ESP.restart();
  });

  serveur.on("/version", []() { // Redemarrage de la carte
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    serveur.send(200, "text/html", "<html lang=fr-FR>\
  <head>\
    <link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />\
    <title>Version</title>\
    <style>\
      body { background-color: #00b449; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\
    </style>\
  </head>\
  <body>\
    <h1>Note de version</h1>\
    <h2>Version du firmware : 1.3.16</h2>\
    <h2>Fonctions disponibles :</h2>\
    <table>\
    <tr>\
        <td><h3>Allumer : </h3></td>\
        <td>\"<a href='../1'>/1</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Eteindre : </h3></td>\
        <td>\"<a href='../0'>/0</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Basculer : </h3></td>\
        <td>\"<a href='../b'>/b</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Etat : </h3></td>\
        <td>\"<a href='../'>/</a>\" ou \"<a href='../etat'>/etat</a>\"</p></td>\
    </tr>\
    <tr>\
        <td><h3>Puissance instantanee en W : </h3></td>\
        <td>\"<a href='../p'>/p</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Energie consommee en J : </h3></td>\
        <td>\"<a href='../e'>/e</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Temps actif : </h3></td>\
        <td>\"<a href='../t'>/t</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Temps actif (jj:hh:mm:ss:mmmm) : </h3></td>\
        <td>\"<a href='../temps'>/temps</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Flush (temps et energie) : </h3></td>\
        <td>\"<a href='../f'>/f</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Adresse IP :</h3></td>\
        <td>\"<a href='../ip'>/ip</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Adresse mac</h3></td>\
        <td>\"<a href='../mac'>/mac</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>RSSID en dBm :</h3></td>\
        <td>\"<a href='../rssi'>/rssi</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>BSSID :</h3></td>\
        <td>\"<a href='../bssid'>/bssid</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Mot de passe de l'AP :</h3></td>\
        <td>\"<a href='../psk'>/psk</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>SSID :</h3></td>\
        <td>\"<a href='../ssid'>/ssid</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Version :</h3></td>\
        <td>\"<a href='../version'>/version</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Redemarrage :</h3></td>\
        <td>\"<a href='../restart'>/restart</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Nom de domaine :</h3></td>\
        <td>\"<a href='../domaine'>/domaine</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Affichage graphique :</h3></td>\
        <td>\"<a href='../graph'>/graph</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Mise a jour du firmware :</h3></td>\
        <td>\"<a href='../firmware'>/firmware</a>\"</td>\
    </tr>\
    <tr>\
        <td><h3>Fichier de description SSDP :</h3></td>\
        <td>\"<a href='../description.xml'>/description.xml</a>\"</td>\
    </tr>\
    </table>\
    <p><a href='http://domogrid.be'>Notre site web DomoGrid </a></p>\
  </body>\
</html>");
  });

  // Modification de l'etat
  serveur.on("/b", []() { // Bascule de l'etat du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    basculeRelais();
  });

  serveur.on("/1", []() { // Activation du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    onRelais();
  });

  serveur.on("/0", []() { // Desactivation du relais
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    offRelais();
  });

  serveur.on("/f", []() { // Mise a zero du compteur d'energie et du temps
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    flushE();
  });

  serveur.on("/graph.svg", []() { // Mise a zero du compteur d'energie et du temps
    #ifdef httpAuth
    if (!serveur.authenticate(prise.getUtilisateur(), prise.getMotdepasse()))
      return serveur.requestAuthentication();
    #endif
    dessinerGraph(puissances, n, millis(), tempsActif(etat(), 0), energie(p, 0));
  });

  //Mise en route du service SSDP
  if (isSecure)  {
    serveur.on("/index.html", HTTP_GET, []() {
      serveur.send(200, "text/plain", "Fonctionne correctement.");
    });

    serveur.on("/description.xml", HTTP_GET, []() {
      SSDP.schema(serveur.client());
    });
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("Prise DomoGrid");
    SSDP.setSerialNumber("000000000000");
    SSDP.setURL("index.html");
    SSDP.setModelName("DomoGrid Smart Plug");
    SSDP.setModelNumber("929000226503");
    SSDP.setModelURL("http://domogrid.be");
    SSDP.setManufacturer("DomoGrid");
    SSDP.setManufacturerURL("http://domogrid.be");
    SSDP.begin();
  }

  // Lancement du serveur
  httpUpdater.setup(&serveur, update_path, prise.getUtilisateur(), prise.getMotdepasse());
  serveur.begin();
  delay(100);
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED)    prise.connectMyWifi();

  else  {
    serveur.handleClient();

    // Variable qui garde en memoire la valeur de la tension lue sur la broche ADC de l'ESP
    // La valeur de la variable est comprise entre 0 et 1023 (10 bits) soit de 0 a 3300 mV
    unsigned int capteur = analogRead(A0);

    // Tension en mV
    Serial.print(map(capteur, 0, 1023, 0 , 3300));
    Serial.print(",");

    // Tension minimale en mV
    Serial.print(map(mini(capteur, frq), 0, 1023, 0 , 3300));
    Serial.print(",");

    // Tension maximale en mV
    Serial.print(map(maxi(capteur, frq), 0, 1023, 0 , 3300));
    Serial.print(",");

    // Tension crete a crete en mV
    Serial.print(uCaC(mini(capteur, frq), maxi(capteur, frq)));
    Serial.print(",");

    // Tension RMS en mV
    Serial.print(uRms(mini(capteur, frq), maxi(capteur, frq)));
    Serial.print(",");

    // Intensite RMS en mV
    Serial.print(iRms(coefi, mini(capteur, frq), maxi(capteur, frq)));
    Serial.print(",");

    // Puissance apparante en VA
    p = VA(coefi, mini(capteur, frq), maxi(capteur, frq));
    Serial.print(p);
    Serial.print(",");

    // Energie consommee en J
    Serial.print(energie(p, 0));
    Serial.println();

    // Periode d'activite du relais en ms
    tempsActif(etat(), 0);
    point(energie(p, 0), puissances, n);
  }
}

/// Bascule l'etat du relais
void basculeRelais()  {
  digitalWrite(broche_relais, !digitalRead(broche_relais));
  serveur.send(204, "");
}

/// Active le relais
void onRelais()  {
  digitalWrite(broche_relais, 1);
  serveur.send(204, "");
}

/// Desactive le relais
void offRelais()  {
  digitalWrite(broche_relais, 0);
  serveur.send(204, "");
}

/// Donne l'etat du relais
bool etat() {
  return digitalRead(broche_relais);
}

/// Determine la valeur minimale de la tension de crete
// Capteur : Valeur a analyser
// freq : Valeur de frequence a laquelle on affiche la valeur min.
unsigned int mini(unsigned int capteur, int freq)  {
  static unsigned int mini = 1023;
  static unsigned long tmps = millis();
  static unsigned int testmin;

  if (capteur < testmin) testmin = capteur;

  if (millis() > (1000 / freq + tmps)) {
    tmps = millis();
    mini = testmin;
    testmin = 1023;
  }
  return mini;
}

/// Determine la valeur maximale de la tension de crete
// Capteur : Valeur a analyser
// freq : Valeur de frequence a laquelle on affiche la valeur max.
unsigned int maxi(unsigned int capteur, int freq)  {
  static unsigned int maxi = 0;
  static unsigned long tmps = millis();
  static unsigned int testmax;

  if (capteur > testmax) testmax = capteur;

  if (millis() > (1000 / freq + tmps)) {
    tmps = millis();
    maxi = testmax;
    testmax = 0;
  }
  return maxi;
}

/// Determine la valeur de la tension crete a crete
unsigned int uCaC(unsigned int mini, unsigned int maxi) { // Tension crete a crete en mV
  return (unsigned int) map((maxi - mini), 0, 1023, 0, 3000);
}

/// Determine la valeur de la tension efficace
unsigned int uRms(unsigned int mini, unsigned int maxi) {
  return (unsigned int) (float) map((maxi - mini), 0, 1023, 0, 3300) / sqrt(8);
}

/// Determine la valeur de l'intensite efficace
float iRms(float coef, unsigned int mini, unsigned int maxi)  {
  return (float) coef * (float) ((float) map((maxi - mini), 0, 1023, 0, 3300) / sqrt(8));
}

/// Determine la valeur de la puissance efficace
float VA(float coef, unsigned int mini, unsigned int maxi) {
  return (float) coef * (float) map((maxi - mini), 0, 1023, 0, 3300) / sqrt(8) * tension;
}

/// Determine la consommation d'ernergie en [J] plus flus
float energie(float puissance, bool flushE) {
  static float energie = 0;
  static float puissance_0 = 0;
  static unsigned long tPasse = 0;
  unsigned long delta_t = millis() - tPasse;

  if (digitalRead(broche_relais)  && puissance > 7.5)   energie += (puissance + puissance_0) / 2 * (float) delta_t / 1000;

  if (energie < 0) energie = 0;

  puissance_0 = puissance;
  tPasse = millis();

  if (flushE) energie = 0;

  return energie;
}

/// Determine la duree de la periode d'activite du relais
unsigned long tempsActif(bool condition, bool flushE) {
  static unsigned long temps = 0;
  static unsigned long temps_0 = 0;

  if (condition) temps += millis() - temps_0;
  temps_0 = millis();
  if (flushE) temps = 0;
  return temps;
}

/// Remet le compteur de consommation a zero
void flushE() {
  tempsActif(etat(), 1);
  energie(p, 1);
  serveur.send(204, "");
}

String getPagePuissance(float p) {
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>Puissance en W</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += (String) p;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageTempsActif(unsigned long t) {
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>Temps actif en ms</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += (String) t;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageTempsActifddhhmmss(unsigned long t) {
  unsigned int s = t / 1000;
  unsigned int m = s / 60;
  unsigned int h = m / 60;
  unsigned int d = h / 24;
  t = t % 1000;
  s = s % 60;
  m = m % 60;
  h = h % 24;

  String mill = " milliseconde.";
  String seco = " seconde, ";
  String minu = " minute, ";
  String heur = " heure, ";
  String jour = " jour, ";

  if (t > 1) mill = " millisecondes.";
  if (s > 1) seco = " secondes, ";
  if (m > 1) minu = " minutes, ";
  if (h > 1) heur = " heures, ";
  if (d > 1) jour = " jours, ";

  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>Temps actif</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += d + jour + h + heur + m + minu + s + seco + t + mill;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageEnergie(float e) {
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>Energie en J</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += (String) e;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageRssi(short int RSSI) {
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>RSSI en dBm</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += (String) RSSI;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageEtat(bool e) {
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='1'/>";
  page += "<title>Etat</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><p>";
  page += (String) e;
  page += "</p>";
  page += "</body></html>";
  return page;
}

String getPageDomaine(String d, String i) {
  String page = "<html lang=fr-FR><head>";
  page += "<title>Nom de domaine</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body><table><tr><td><p>Nom de domaine :</p></td><td>\"<a href='http://";
  page += d;
  page += ".local'>http://";
  page += d;
  page += ".local</a>\"</td></tr>";
  page += "<tr><td><p>Nom de domaine :</p></td><td>\"<a href='http://";
  page += d;
  page += ".host'>http://";
  page += d;
  page += ".host</a>\"</td></tr>";
  page += "<tr><td><p>Adresse IP :</p></td><td>\"<a href='http://";
  page += i;
  page += "'>http://";
  page += i;
  page += "</a>\"</td></tr>";
  page += "</table>";
  page += "</body></html>";
  return page;
}

String getPageGraph() {
  String page = "<html lang=fr-FR><head>"; // <meta http-equiv='refresh' content='10'/>";
  page += "<title>Affichage graphique</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "<style> body { background-color: #00b449; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }</style>";
  page +=        "<script language='JavaScript'>";
  page +=          "function refreshIt(element) {";
  page +=              "setTimeout(function() {";
  page +=                  "element.src = element.src.split('?')[0] + '?' + new Date().getTime();";
  page +=                  "refreshIt(element);";
  page +=            "}, 5000);"; // refresh every 50ms
  page +=         "}";
  page +=    "</script>";
  page += "</head><body><h1>Graphique :</h1>";
  page += "<img src='../graph.svg' name='Graphique' onload='refreshIt(this)'>";
  //page += "<img src=\"/graph.svg\" />";

  page += "<iframe src='../data' width='600' height='200'>";
  page += "<p>Votre navigateur ne supporte pas les iframes.</p>";
  page += "</iframe>";

  page += "<h1>Modifier l'etat :</h1><table>";
  page += "<tr><td><p><a href='../b'><button>Basculer</button></a></p></td>";
  page += "<td><p><a href='../1'><button>On</button></a></p></td>";
  page += "<td><p><a href='../0'><button>Off</button></a></p></td>";
  page += "<td><p><a href='../f'><button>Remise a zero</button></a></p></td></tr>";
  page += "</table>";
  page += "<p><a href='http://domogrid.be' target='_blank'>Notre site web DomoGrid </a></p>";
  page += "</body></html>";

  return page;
}

void dessinerGraph(float tab[], int nb, unsigned long t, unsigned long t_e, float e) {
  float Pm = 0;
  if (t_e) Pm = e/(t_e/1000);
  unsigned int s = t / 1000;
  unsigned int m = s / 60;
  unsigned int h = m / 60;
  unsigned int d = h / 24;
  unsigned long t2;
  if (t < 600000) t2 = 0;
  else t2 = t - 600000;
  t = t % 1000;
  s = s % 60;
  m = m % 60;
  h = h % 24;

  String mill = " milliseconde";
  String seco = " seconde, ";
  String minu = " minute, ";
  String heur = " heure, ";
  String jour = " jour, ";

  if (t > 1) mill = " millisecondes";
  if (s > 1) seco = " secondes, ";
  if (m > 1) minu = " minutes, ";
  if (h > 1) heur = " heures, ";
  if (d > 1) jour = " jours, ";

  unsigned int s2 = t2 / 1000;
  unsigned int m2 = s2 / 60;
  unsigned int h2 = m2 / 60;
  unsigned int d2 = h2 / 24;
  t2 = t2 % 1000;
  s2 = s2 % 60;
  m2 = m2 % 60;
  h2 = h2 % 24;

  String mill2 = " milliseconde.";
  String seco2 = " seconde, ";
  String minu2 = " minute, ";
  String heur2 = " heure, ";
  String jour2 = " jour, ";

  if (t2 > 1) mill2 = " millisecondes.";
  if (s2 > 1) seco2 = " secondes, ";
  if (m2 > 1) minu2 = " minutes, ";
  if (h2 > 1) heur2 = " heures, ";
  if (d2 > 1) jour2 = " jours, ";

  String out;
  char temp[100];
  out = "";
  out = "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"";
  out += (String) nb;
  out += "\" height=\"520\">\n";
  out += "<rect width=\"";
  out += (String) nb;
  out += "\" height=\"520\" fill=\"rgb(255, 255, 255)\" stroke-width=\"1\" stroke=\"rgb(0, 180, 73)\" />\n";
  out += "<g stroke=\"rgb(0, 180, 73)\">\n";
  //out += "<line x1=\"0\" y1=\"0\" x2=\"400\" y2=\"120\" stroke-width=\"1\" />";
  float max = 0;
  float moye = 0;
  for (int i(0); i < nb-1; i++)  {
    moye += (tab[i] + tab[i + 1]);
  }
  for (int i(0); i < nb-1; i+=2)  {
    if(max<(tab[i]+tab[i+1])) max=(tab[i]+tab[i+1]);
  }
  max/=2;
  if(max<Pm) max = Pm;
  for (int x = 0; x < nb; x += 2) {
    int x1 = nb - x;
    int x2 = x + 2;
    int y1 = 420 - 400 * (tab[x] + tab[x + 1]) / (2 * max);
    int y2 = 420 - 400 * (tab[x2] + tab[x2 + 1]) / (2 * max);
    x2 = nb - x2;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x1, y1, x2, y2);
    out += temp;
  }
  out += "</g>\n";
  int x;
  if (millis() < 600000) x = millis()/1000;
  else x = nb;

  int moy(0), pm(0); 
  if(moye)  {
    moye /= ((float)x * 2);
    moy = (int)(420 - (400 * moye) / max);
  }
  
  if(Pm)  pm = (int)(420 - (400 * Pm) / max);
  
  sprintf(temp, "<g stroke=\"rgb(0, 0, 0)\"><line x1=\"0\" y1=\"420\" x2=\"0\" y2=\"0\" stroke-width=\"2\" />\n");
  out += temp;
  sprintf(temp, "<line x1=\"0\" y1=\"420\" x2=\"%d\" y2=\"420\" stroke-width=\"2\" /></g>\n", nb);
  out += temp;
  sprintf(temp, "<g stroke=\"rgb(255, 0, 0)\"><line x1=\"0\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"2\" /></g>\n", moy , (int) x, moy);
  out += temp;
  sprintf(temp, "<g stroke=\"rgb(255, 144, 0)\"><line x1=\"0\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"2\" /></g>\n", pm , (int) x, pm);
  out += temp;
  sprintf(temp, "<g stroke=\"rgb(168, 168, 168)\"><line x1=\"20\" y1=\"0\" x2=\"%d\" y2=\"0\" stroke-width=\"1\" />\n", nb);
  out += temp;
  sprintf(temp, "<line x1=\"0\" y1=\"120\" x2=\"%d\" y2=\"120\" stroke-width=\"1\" />\n", nb);
  out += temp;
  sprintf(temp, "<line x1=\"0\" y1=\"220\" x2=\"%d\" y2=\"220\" stroke-width=\"1\" />\n", nb);
  out += temp;

  x = nb / 5;
  sprintf(temp, "<line x1='%d' y1='420' x2='%d' y2='20' stroke-width='1' />\n", x, x);
  out += temp;
  sprintf(temp, "<line x1='%d' y1='420' x2='%d' y2='20' stroke-width='1' />\n", 2 * x, 2 * x);
  out += temp;
  sprintf(temp, "<line x1='%d' y1='420' x2='%d' y2='20' stroke-width='1' />\n", 3 * x, 3 * x);
  out += temp;
  sprintf(temp, "<line x1='%d' y1='420' x2='%d' y2='20' stroke-width='1' />\n", 4 * x, 4 * x);
  out += temp;
  sprintf(temp, "<line x1='%d' y1='420' x2='%d' y2='20' stroke-width='1' />\n", nb, nb);
  out += temp;

  sprintf(temp, "<line x1=\"0\" y1=\"320\" x2=\"%d\" y2=\"320\" stroke-width=\"1\" /></g>\n", nb);
  out += temp;
  moy -= 5;
  pm -=5;
  out += "<text x='5' y='";
  out += (String) moy;
  out += "' id='text4739'>Moyenne sur la période : ";
  out += (String) moye;
  out += " W</text>";
  out += "<text x='5' y='";
  out += (String) pm;
  out += "' id='text4740'>Moyenne : ";
  out += (String) Pm;
  out += " W</text>\n";
  out += "<text x='5' y='15' id='text4740'>Puissance en Watts (0 W- ";
  out += (String) max;
  out += " W)</text>";
  out += "<text x='190' y='450' id='text4741'>Temps (jj, hh, mm, ss, mmm)</text>";
  out += "<text x='190' y='470' id='text4742'>(";
  out += d + jour + h + heur + m + minu + s + seco + t + mill;
  out += " - </text><text x='190' y='490' id='text4743'>";
  out += d2 + jour2 + h2 + heur2 + m2 + minu2 + s2 + seco2 + t2 + mill2;
  out += ")</text>";
  out += "</svg>\n";

  serveur.send ( 200, "image/svg+xml", out);
}

void point(float e, float tab[], unsigned int nb)  {
  static unsigned long t = millis();
  static float e_pre = 0;

  if (millis() - t >= 1000)  {
    for (int i(0); i < (nb - 1); i++) {
      tab[i] = tab[i + 1];
    }
    tab[nb - 1] = (e - e_pre);
    if(tab[nb - 1]<0) tab[nb - 1]=0;
    t = millis();
    e_pre = e;
  }
}

//String getActualGraph() {
//  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='10'/>";
//  page += "<title>Graphique</title>";
//  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
//  page += "</head><body>";
//  page += "<img src=\"/graph.svg\" />";
//  page += "</body></html>";
//  return page;
//}

String getData(unsigned long t, float pui, float e, bool etat) {
  unsigned int s = t / 1000;
  unsigned int m = s / 60;
  unsigned int h = m / 60;
  unsigned int d = h / 24;
  unsigned long t2 = t / 1000;
  t = t % 1000;
  s = s % 60;
  m = m % 60;
  h = h % 24;

  String mill = " milliseconde.";
  String seco = " seconde, ";
  String minu = " minute, ";
  String heur = " heure, ";
  String jour = " jour, ";

  if (t > 1) mill = " millisecondes.";
  if (s > 1) seco = " secondes, ";
  if (m > 1) minu = " minutes, ";
  if (h > 1) heur = " heures, ";
  if (d > 1) jour = " jours, ";

  String ETAT = "Off";
  if (etat) ETAT = "On";
  else pui = 0;

  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='2'/>";
  page += "<title>Graphique</title>";
  page += "<link rel='icon' type='image/png' href='http://domogrid.be/ressources/favicon/favicon.ico' />";
  page += "</head><body>";
  page += "<h1>Donnees :</h1><table>";
  page += "<tr><td><p>Etat :</p></td><td>";
  page += (String) etat;
  page += " ou ";
  page += ETAT;
  page += "</td></tr>";
  page += "<tr><td><p>Puissance instantanee :</p></td><td>";
  page += (String) pui;
  page += " W</td></tr>";
  page += "<tr><td><p>Energie consommee :</p></td><td>";
  page += (String) e;
  page += " J, soit ";
  page += (String) (((double) e) / (1000 * 3600));
  page += " kWh</td></tr>";
  page += "<tr><td><p>Temps actif :</p></td><td>";
  page += d + jour + h + heur + m + minu + s + seco + t + mill;
  page += "</td></tr>";
  page += "<tr><td><p>Puissance moyenne :</p></td><td>";
  page += (String) (e / (float)t2);
  page += " W</td></tr>";
  page += "</table>";
  page += "</body></html>";
  return page;
}

//void surintensite(unsigned float i) {
//  if(i>16) relass
//}
