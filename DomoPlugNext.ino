/*
 * Code realise par Vincent STRAGIER pour le projet DomoGrid (2017)
 * 
 * Le but de ce code est de controler une prise relais 230V (16A) a l'aide d'un ESP8266 ESP-12
 * On retourne aussi l'etat du relais et de la prise (on ou off, periode d'activite du relais, puissance consommee)
 * Ici, on utilise le capteur AC1015 pour effectuer la mesure du courant
 * 
 * Cette prise ne permet pas de mesurer precissement la puissance consommee.
 * Effectivement, on ne mesure pas la tension et on ne peut donc pas avoir le cos(Phi) ou la valeur exacte de la tension 
 * La tension n'est pas mesuree, car il n'y a qu'une seule entree ADC sur l'ESP-12 pour mesurer la valeur d'une tension analogique.
 */

/*
 * Ajouter du WPS
 * Ajouter le mesh
 * Ajouter de l'https
 * Ajouter de un compte (users logging)
 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

// Frequence de rafraichissement du calcul des valeurs extremes de la tension
#define frq 49

// Tension efficace du reseau electrique
#define tension 227

// Coefficient propre au capteur d'intensite (rapport entre la tension mesuree et l'intensite supposee)
#define coefi 0.01468428781204111600587371512482

// Broche a laquelle la commande du relais est connectee
uint8_t broche_relais = 0;

// Objet ESP8266WebServer
ESP8266WebServer serveur;

// Informations de la connexion
char* ssid = "ASUS";
char* mdp  = "hacb-d8tw-m6zc";

// Puissance en Watts
float p = 0;

// Satut du module WiFi
int status = WL_IDLE_STATUS;

void setup() {
  pinMode(broche_relais, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  // La DEL integree clignote tant que la connexion n'est pas etablie
  if(WiFi.status() != WL_CONNECTED)    connectMyWifi(ssid, mdp);
  
  Serial.print("\n Adresse IP : ");
  // Retourne d'adresse IP du module
  Serial.println(WiFi.localIP());

  // Definission du nom de domaine a diffuser sur le reseau
  if (!MDNS.begin("domogrid_v4")) {
    Serial.println("Erreur de configuration du repondeur mDNS!");
    while(1) {  delay(1000);  }
   }
  Serial.println("Repondeur mDNS demarre");

  // Definition des urls possibles
  // Obtention de l'etat
  serveur.on("/",[](){serveur.send(200, "text/plain", (String) etat());}); // etat du relais
  serveur.on("/etat",[](){serveur.send(200, "text/plain", (String) etat());}); // etat du relais
  serveur.on("/p",[](){serveur.send(200, "text/plain", (String) p);}); // puissance actuelle en W
  serveur.on("/e",[](){serveur.send(200, "textplain", (String) energie(p,0));}); // energie consommee en J
  serveur.on("/t",[](){serveur.send(200, "text/plain", (String)  tempsActif(etat(),0));}); // periode d'activite du relais
  serveur.on("/w",[](){serveur.send(200, "text/plain", (String)  WiFi.RSSI());}); // puissance du signal en dBm
  
  // Modification de l'etat
  serveur.on("/b", basculeRelais);  // Bascule de l'etat du relais
  serveur.on("/1", onRelais);   // Activation du relais
  serveur.on("/0", offRelais);   // Desactivation du relais
  serveur.on("/f", flushE);   // Mise a zero du compteur et du temps

  // Lancement du serveur
  serveur.begin();

  digitalWrite(broche_relais, 0);
  delay(100);
}

void loop() {
  if(WiFi.status() != WL_CONNECTED)    connectMyWifi(ssid, mdp);
  
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
  Serial.print(uCaC(mini(capteur, frq),maxi(capteur, frq)));
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
  Serial.print(energie(p,0));
  Serial.println();

  // Periode d'activite du relais en ms
  tempsActif(etat(),0);
}
}

/// Bascule l'etat du relais
void basculeRelais()  {
  digitalWrite(broche_relais, !digitalRead(broche_relais));
  serveur.send(204, "Fait!");
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
bool etat() { //Donne l'etat du relais
 return digitalRead(broche_relais); 
}

/// Determine la valeur minimale de la tension de crete
// Capteur : Valeur a analyser
// freq : Valeur de frequence a laquelle on affiche la valeur min.
unsigned int mini(unsigned int capteur, int freq)  {
  static bool unefois = true;
  static unsigned int mini;                           
  static unsigned long tmps;
  static unsigned int testmin;
  
  if(unefois) {
    mini = 1023;                              
    tmps = millis();
    unefois = false;
  }

  if(capteur < testmin) testmin = capteur;

  if(millis() > (1000/freq + tmps)) {
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
  static bool unefois = true;
  static unsigned int maxi;
  static unsigned long tmps;
  static unsigned int testmax;
  
  if(unefois) {
    maxi = 0;                 
    tmps = millis();
    unefois = false;
  }

  if(capteur > testmax) testmax = capteur;

  if(millis() > (1000/freq + tmps)) {
    tmps = millis();
    maxi = testmax;
    testmax = 0;
  }
  return maxi;
}

/// Determine la valeur de la tension crete a crete
unsigned int uCaC(unsigned int mini, unsigned int maxi) { // Tension crete a crete en mV
  return (unsigned int) map((maxi-mini), 0, 1023, 0, 3000);
}

/// Determine la valeur de la tension efficace
unsigned int uRms(unsigned int mini, unsigned int maxi) {
  return (unsigned int) (float) map((maxi-mini), 0, 1023, 0, 3300)/sqrt(8);
}

/// Determine la valeur de l'intensite efficace
float iRms(float coef, unsigned int mini, unsigned int maxi)  {
  return (float) coef * (float) ((float) map((maxi-mini), 0, 1023, 0, 3300)/sqrt(8));
}

/// Determine la valeur de la puissance efficace
float VA(float coef, unsigned int mini, unsigned int maxi) {
  return (float) coef * (float) map((maxi-mini), 0, 1023, 0, 3300)/sqrt(8) * tension;
}

/// Determine la consommation d'ernergie en [J] plus flus
float energie(float puissance, bool flushE) {
  static float energie = 0;
  static float puissance_0 = 0;
  static unsigned long tPasse = 0;
  unsigned long delta_t = millis()-tPasse;

  if(digitalRead(broche_relais)  && puissance > 7.5)   energie += (puissance + puissance_0)/2 * (float) delta_t/1000;
   
  if(energie < 0) energie = 0;
  
  puissance_0 = puissance;
  tPasse = millis();
  
  if(flushE) energie = 0;

  return energie;
}

/// Determine la duree de la periode d'activite du relais
unsigned long tempsActif(bool condition, bool flushE) {
  static unsigned long temps = 0;
  static unsigned long temps_0 = 0;

  if(condition) temps += millis() - temps_0;
  temps_0 = millis();
  
  if(flushE) temps = 0;
  
  return temps;
}

void flushE() {
  tempsActif(etat(), 1);
}

bool connectMyWifi(char* ssid, char* mdp)  {
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
