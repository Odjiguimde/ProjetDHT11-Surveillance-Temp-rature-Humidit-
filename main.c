/*
 * dht11_projet.c
 */

#include <avr/io.h>
#define F_CPU 1000000UL
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>

#include "lcd_4bits.h"

// === Définition des broches pour LEDs ===

#define led_rouge PC1
#define led_jaune PH4
#define led_verte PK5

// === Capteur DHT11 ===

#define DHT11_PIN PD7

// === Boutons physiques ===

#define BTN_UP PD0 // Augmenter les seuils
#define BTN_DOWN PD2 // Diminuer les seuils
#define BTN_TEMP_HUM PB3 // Car il n'y a pas d'interruption sur le port PH3 : // Changer entre température et humidité
#define BTN_OK PE4 // Valider / quitter le mode réglage

// Variables globales

// === Variables globales pour état des boutons (modifiées dans les interruptions) ===

volatile uint8_t btn_up = 0;
volatile uint8_t btn_down = 0;
volatile uint8_t btn_temp_hum = 0;
volatile uint8_t btn_ok = 0;

// === États de fonctionnement ===

uint8_t mode_reglage = 0;
uint8_t type_selection = 0;
uint8_t seuil_selection = 0;

// === Valeurs lues ===

uint8_t temp_actuelle = 25;
uint8_t hum_actuelle = 60;

// === Seuils par défaut ===

uint8_t temp_sup = 30;
uint8_t temp_inf = 20;
uint8_t hum_sup = 70;
uint8_t hum_inf = 50;

// === Buffer pour affichage LCD ===
char buffer[32];

// === Prototypes de fonctions ===

void init_ports(void);
void mesure_temp_hum(void);
void reglage_alarme(void);
void alarme(void);
uint8_t lire_dht11(uint8_t *temp, uint8_t *hum);

// === Initialisation des ports et des interruptions ===

void init_ports(void)
{	
	// LEDs en sortie
    DDRC |= (1 << led_rouge);
    DDRH |= (1 << led_jaune);
    DDRK |= (1 << led_verte);
	
	// Éteindre les LEDs au départ
    PORTC &= ~(1 << led_rouge);
    PORTH &= ~(1 << led_jaune);
    PORTK &= ~(1 << led_verte);
	
	// Boutons en entrée avec pull-up
    DDRD &= ~((1 << BTN_UP) | (1 << BTN_DOWN));
    DDRE &= ~(1 << BTN_OK);
    DDRH &= ~(1 << BTN_TEMP_HUM);
	
    PORTD |= ((1 << BTN_UP) | (1 << BTN_DOWN));
    PORTE |= (1 << BTN_OK);
    PORTH |= (1 << BTN_TEMP_HUM);

	// Configuration des interruptions externes
    EICRA = (1 << ISC01) | (1 << ISC21);// INT0, INT2 sur front descendant
    EICRB = (1 << ISC41);				// INT4 sur front descendant
    EIMSK = (1 << INT0) | (1 << INT2) | (1 << INT4);// Activer INT0, INT2, INT4
	// Interruption Pin Change pour PB3
    PCICR = (1 << PCIE0);  // Autoriser interruptions sur PCINT0 (PB)
    PCMSK0 = (1 << PCINT3);// Activer PCINT3 pour PB3
    sei();
}

// === Gestion des interruptions ===
ISR(INT0_vect) { btn_up = 1; }
ISR(INT2_vect) { btn_down = 1; }
ISR(INT4_vect) { btn_ok = 1; }
ISR(PCINT0_vect) { btn_temp_hum = 1; }

// === Lecture du capteur DHT11 (Fusion de start_dht11 reponse_dht11 et transmission_dht11 ) ===
uint8_t lire_dht11(uint8_t *temp, uint8_t *hum)
{
    uint8_t dht_data[5] = {0};
    uint8_t i, j;

	// Envoi du signal de démarrage(start_dht11)
    DDRD |= (1 << DHT11_PIN);
    PORTD &= ~(1 << DHT11_PIN);
    _delay_ms(20);
    PORTD |= (1 << DHT11_PIN);
    _delay_us(20);
    DDRD &= ~(1 << DHT11_PIN);

	// Attente de la réponse(reponse_dht11)
    while(PIND & (1 << DHT11_PIN));
    while(!(PIND & (1 << DHT11_PIN)));
    while(PIND & (1 << DHT11_PIN));

	// Lecture des 5 octets (40 bits) (transmission_dht11)
    for(i = 0; i < 5; i++) {
        uint8_t tampon = 0;
        for(j = 0; j < 8; j++) {
            while(!(PIND & (1 << DHT11_PIN)));
            _delay_us(30);
            if(PIND & (1 << DHT11_PIN))
                tampon = (tampon << 1) | 1;
            else
                tampon = (tampon << 1);
            while(PIND & (1 << DHT11_PIN));
        }
        dht_data[i] = tampon;
    }

	 // Vérification du checksum
    if ((uint8_t)(dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) == dht_data[4]) {
        *hum = dht_data[0];
        *temp = dht_data[2];
        return 1;
    }

    return 0;
}

// === Affichage des mesures sur LCD ===
void mesure_temp_hum(void)
{
    PORTK |= (1 << led_verte);// LED verte allumée (fonctionnement normal)
	
    if (lire_dht11(&temp_actuelle, &hum_actuelle)) {
        cls_LCD();
        pos_xy(1, 1);
        sprintf(buffer, "Temp = %d C", temp_actuelle);
        Write_LCD(buffer);
        pos_xy(1, 2);
        sprintf(buffer, "Humi = %d %%", hum_actuelle);
        Write_LCD(buffer);
    }
}

// === Mode réglage des seuils ===
void reglage_alarme(void)
{
    PORTK &= ~(1 << led_verte);// Éteindre LED verte pour indiquer réglage
	
    while(1)
    {
		// Quitter le mode réglage si bouton OK maintenu
        if(btn_ok) {
            btn_ok = 0;
            _delay_ms(1000);
            if(!(PINE & (1 << BTN_OK))) {
                mode_reglage = 0;
                return;
            }
        }
		// Bascule entre température et humidité
        if(btn_temp_hum) {
            btn_temp_hum = 0;
            type_selection = !type_selection;
        }

		// Augmentation des seuils
        if(btn_up) {
            btn_up = 0;
            _delay_ms(1000);
            if(!(PIND & (1 << BTN_UP))) {
                seuil_selection = 0;// Sélectionner seuil haut
            } else {
                if(type_selection == 0) {
					// Température
                    if(seuil_selection == 0 && temp_sup < 50) temp_sup++;
                    else if(seuil_selection == 1 && temp_inf < temp_sup) temp_inf++;
                } else {
					// Humidité
                    if(seuil_selection == 0 && hum_sup <= 90) hum_sup += 10;
                    else if(seuil_selection == 1 && hum_inf <= hum_sup) hum_inf += 10;
                }
            }
        }

		// Diminution des seuils
        if(btn_down) {
            btn_down = 0;
            _delay_ms(1000);
            if(!(PIND & (1 << BTN_DOWN))) {
                seuil_selection = 1;// Sélectionner seuil bas
            } else {
                if(type_selection == 0) {
                    if(seuil_selection == 0 && temp_sup > temp_inf) temp_sup--;
                    else if(seuil_selection == 1 && temp_inf > 0) temp_inf--;
                } else {
                    if(seuil_selection == 0 && hum_sup >= hum_inf + 10) hum_sup -= 10;
                    else if(seuil_selection == 1 && hum_inf >= 10) hum_inf -= 10;
                }
            }
        }

		// Affichage LCD des seuils
        cls_LCD();
        pos_xy(1, 1);
        if(seuil_selection == 0) Write_LCD("Alarme Seuil_SUP");
        else Write_LCD("Alarme Seuil_INF");

        pos_xy(1, 2);
        if(type_selection == 0) {
            if(seuil_selection == 0)
                sprintf(buffer, "TEMP_SUP = %dC", temp_sup);
            else
                sprintf(buffer, "TEMP_INF = %dC", temp_inf);
        } else {
            if(seuil_selection == 0)
                sprintf(buffer, "HUMI_SUP = %d%%", hum_sup);
            else
                sprintf(buffer, "HUMI_INF = %d%%", hum_inf);
        }
        Write_LCD(buffer);

        _delay_ms(200);
    }
}

// === Gestion de l'alarme ===
void alarme(void)
{
    uint8_t compteur_temp = 0;
	
	// Vérification si la température est hors seuil pendant 3 mesures consécutives
    for (uint8_t i = 0; i < 3; i++) {
        lire_dht11(&temp_actuelle, &hum_actuelle);
        if (temp_actuelle > temp_sup || temp_actuelle < temp_inf)
            compteur_temp++;
        _delay_ms(200);
    }

	// Si température anormale confirmée
    if (compteur_temp >= 3) {
        PORTK &= ~(1 << led_verte);
        while (1) {
            lire_dht11(&temp_actuelle, &hum_actuelle);
            if (temp_actuelle <= temp_sup && temp_actuelle >= temp_inf)
                break;

            cls_LCD();
            pos_xy(1, 1);
            Write_LCD("!!!ALARME!!!");
            pos_xy(1, 2);
            sprintf(buffer, "TEMP = %d C", temp_actuelle);
            Write_LCD(buffer);
            PORTC ^= (1 << led_rouge);
            _delay_ms(300);
        }
        PORTC &= ~(1 << led_rouge);
        PORTK |= (1 << led_verte);
    }

	// Vérification humidité
    uint8_t compteur_hum = 0;
    for (uint8_t i = 0; i < 3; i++) {
        lire_dht11(&temp_actuelle, &hum_actuelle);
        if (hum_actuelle > hum_sup || hum_actuelle < hum_inf)
            compteur_hum++;
        _delay_ms(200);
    }

    if (compteur_hum >= 3) {
        PORTK &= ~(1 << led_verte);
        while (1) {
            lire_dht11(&temp_actuelle, &hum_actuelle);
            if (hum_actuelle <= hum_sup && hum_actuelle >= hum_inf)
                break;

            cls_LCD();
            pos_xy(1, 1);
            Write_LCD("!!!ALARME!!!");
            pos_xy(1, 2);
            sprintf(buffer, "HUMI = %d %%", hum_actuelle);
            Write_LCD(buffer);
            PORTH ^= (1 << led_jaune);
            _delay_ms(300);
        }
        PORTH &= ~(1 << led_jaune);
        PORTK |= (1 << led_verte);
    }
}

// === Fonction principale ===
int main(void)
{
    init_ports();
    init_LCD();
    cls_LCD();

    while(1)
    {
		mesure_temp_hum();
		
		// Entrée en mode réglage si combinaison de boutons détectée
        if((btn_up && btn_down) || btn_temp_hum) {
            btn_up = 0;
            btn_down = 0;
            btn_temp_hum = 0;
            mode_reglage = 1;
        }

		// Mode réglage actif
        if(mode_reglage) {
	        reglage_alarme();
			// Lancement alarme si seuil dépassé après réglage
	         if(hum_actuelle > hum_sup || hum_actuelle < hum_inf || temp_actuelle >= temp_sup || temp_actuelle <= temp_inf ) {
	        alarme();
			 }
        }

        _delay_ms(1000);
    }
}

