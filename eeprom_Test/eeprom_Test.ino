#include <EEPROM.h>

String str_scale_fix;
float scale_fix = -11800.2f;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  str_scale_fix = String(scale_fix, 1);
  int str_length = str_scale_fix.length();

  Serial.printf("Eis a porra %s, cujo tamanho diz ser %d\n", str_scale_fix.c_str(), str_length);

  //No endereço ZERO salva o tamanho
  EEPROM.write(0, str_length);
  
  for (int p = 0; p < str_length; p++) {
    EEPROM.write(p+1, str_scale_fix[p]);
  }

  EEPROM.commit();
  Serial.println("Wrote!");

  int tam_lido = EEPROM.read(0);
  Serial.printf("Eis a porra do tamanho lido %d\n", tam_lido);

  char str_read_scale_fix[tam_lido + 1];

  int p;
  for (p = 0; p < tam_lido; p++) {
    str_read_scale_fix[p] = EEPROM.read(p+1);
  }
  str_read_scale_fix[p] = 0;
  Serial.printf("Eis a porra do scale_fix lido %s\n", str_scale_fix.c_str());

  EEPROM.end();
  Serial.println("Done!");
}

void loop() {
}
