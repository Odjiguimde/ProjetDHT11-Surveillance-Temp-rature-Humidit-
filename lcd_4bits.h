/* Pilote pour LCD alphanumérique 16x2 */
/* Adapté par DJIGUIMDE Oumaro Titans */
#include <string.h>
#include <stdio.h>

// Définitions pour le LCD (16x2)
#define LCD_D4  0x01    // PA0
#define LCD_D5  0x02    // PA1
#define LCD_D6  0x04    // PA2
#define LCD_D7  0x08    // PA3
#define LCD_RS  0x10    // PA4
#define LCD_RW  0x20    // PA5
#define LCD_EN  0x40    // PA6

#define LCD_length     16  // 16 caractères par ligne
#define LCD_lines      2   // 2 lignes

// Fonctions de base pour le LCD en mode 4 bits

void Write_Cmd1(unsigned char value) {
	PORTA &= ~(LCD_EN + LCD_RS + LCD_RW);
	PORTA = (value & (0xF0)) >> 4;
	PORTA |= LCD_EN;
	_delay_us(5);
	PORTA &= ~LCD_EN;
	_delay_us(5);
}

void Write_Cmd(unsigned char value) {
	PORTA &= ~(LCD_EN + LCD_RS + LCD_RW);
	PORTA = (value & (0xF0)) >> 4;
	PORTA |= LCD_EN;
	_delay_us(5);
	PORTA &= ~LCD_EN;
	_delay_us(5);
	PORTA = value & 0x0F;
	PORTA |= LCD_EN;
	_delay_us(5);
	PORTA &= ~LCD_EN;
	_delay_us(5);
}

/* Initialisation du LCD en mode 4 bits */
void init_LCD(void) {
	DDRA |= LCD_EN + LCD_RS + LCD_RW;
	DDRA |= LCD_D4 + LCD_D5 + LCD_D6 + LCD_D7;
	_delay_ms(15);
	Write_Cmd1(0x30);
	_delay_ms(5);
	Write_Cmd1(0x30);
	_delay_us(100);
	Write_Cmd1(0x30);
	_delay_us(50);
	Write_Cmd1(0x20);
	_delay_us(50);
	Write_Cmd(0x28); // Mode 4 bits, 2 lignes
	_delay_us(50);
	Write_Cmd(0x08);
	_delay_us(50);
	Write_Cmd(0x01);
	_delay_ms(2);
	Write_Cmd(0x06);
	_delay_us(50);
	Write_Cmd(0x0F);
	_delay_us(50);
}

void Write_Data(unsigned char value) {
	PORTA &= ~(LCD_EN + LCD_RW);
	PORTA |= LCD_RS;
	PORTA = (value & (0xF0)) >> 4;
	PORTA |= LCD_RS;
	PORTA |= LCD_EN;
	_delay_us(100);
	PORTA &= ~LCD_EN;
	_delay_us(100);
	PORTA = value & 0x0F;
	PORTA |= LCD_RS;
	PORTA |= LCD_EN;
	_delay_us(100);
	PORTA &= ~LCD_EN;
	_delay_us(100);
}

void Write_LCD(char message[]) {
	unsigned char i;
	for(i = 0; i < strlen(message); i++) {
		Write_Data(message[i]);
		_delay_us(150);
	}
}

void ligne_1(unsigned char value) {
	Write_Cmd(0x80 + (value-1)); // Adresse ligne 1 (16x2)
	_delay_us(50);
}

void ligne_2(unsigned char value) {
	Write_Cmd(0xC0 + (value-1)); // Adresse ligne 2 (16x2)
	_delay_us(50);
}

void pos_xy(unsigned char x, unsigned char y) {
	if (x <= LCD_length && y <= LCD_lines) {
		switch (y) {
			case 1: ligne_1(x); break;
			case 2: ligne_2(x); break;
		}
	}
	_delay_ms(2);
}

void cls_LCD(void) {
	Write_Cmd(0x01);
	_delay_ms(2);
}