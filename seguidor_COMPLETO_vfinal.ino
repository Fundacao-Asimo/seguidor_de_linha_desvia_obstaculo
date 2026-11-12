// --- PINOS DOS MOTORES (L298N) ---
#define enA 10 // Enable1 L298 Pin enA
#define in1 9  // Motor1  L298 Pin in1
#define in2 8  // Motor1  L298 Pin in2
#define in3 7  // Motor2  L298 Pin in3
#define in4 6  // Motor2  L298 Pin in4
#define enB 5  // Enable2 L298 Pin enB

// --- PINOS DO MÓDULO 5 CANAIS TCRT5000 ---
// Olhando o robô por trás (da esquerda para a direita)
#define S1 A0 // Sensor Extremo Esquerdo
#define S2 A1 // Sensor Meio Esquerdo
#define S3 A2 // Sensor Central
#define S4 A3 // Sensor Meio Direito
#define S5 A4 // Sensor Extremo Direito

// --- SENSORES DE OBSTÁCULO E CONTROLES ---
#define echo 3      // Pino Echo do Ultrassonico (NOVO PINO)
#define trigger 4   // Pino Trigger do Ultrassonico (NOVO PINO)
#define servo 11    // Pino do Servo (NOVO PINO)

#define POT_PIN A5    // Pino do Potenciômetro
#define CHAVE_MODO 2  // Chave Seletora de Modos

// --- CONFIGURAÇÃO DE LEITURA DO SENSOR IR ---
// A maioria desses módulos lê 0 (LOW) no PRETO e 1 (HIGH) no BRANCO.
// Se o seu for ao contrário, basta mudar LINHA para 1 e FUNDO para 0.
#define LINHA 0
#define FUNDO 1

int Set = 15;
int Set_Livre = 30; // Nova distância de segurança maior para o modo livre
int distance_L, distance_F, distance_R;
int baseSpeed = 110; 

void setup() {
  Serial.begin(9600);

  // Configura a Chave e o Potenciômetro
  pinMode(CHAVE_MODO, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  // Configura os 5 Sensores de Linha
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  // Configura Ultrassônico e Servo
  pinMode(echo, INPUT);
  pinMode(trigger, OUTPUT);
  pinMode(servo, OUTPUT);

  // Configura os Motores
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(enB, OUTPUT);
  
  // Printar o modo no serial logo ao ligar
  if (digitalRead(CHAVE_MODO) == HIGH) { 
    Serial.println("MODO SEGUIDOR DE LINHA INICIADO");
  } else {
    Serial.println("MODO LIVRE INICIADO");
  }

  // "Dança" inicial do servo
  for (int angle = 70; angle <= 140; angle += 5) { servoPulse(servo, angle); }
  for (int angle = 140; angle >= 0; angle -= 5) { servoPulse(servo, angle); }
  for (int angle = 0; angle <= 70; angle += 5) { servoPulse(servo, angle); }

  distance_F = Ultrasonic_read();
  delay(500);
}

void loop() {
  // Lê o potenciômetro e ajusta a velocidade base
  int potValue = analogRead(POT_PIN);
  baseSpeed = map(potValue, 0, 1023, 70, 220); // Valor max ajustado para 220

  // Lê a chave no loop para atualizar em tempo real!
  int modo = digitalRead(CHAVE_MODO);

  if (modo == HIGH) { 
    modoSeguidorDeLinha();
  } else {
    modoDesviadorLivre();
  }
}

// =======================================================
//                MODO 1: SEGUIDOR DE LINHA
// =======================================================
void modoSeguidorDeLinha() {
  distance_F = Ultrasonic_read();

  if (distance_F > Set) {
    // Caminho livre, segue a linha com os 5 canais
    seguirLinha5Canais();
  } else {
    // Obstáculo detectado! Aciona a rotina de desvio
    Check_side_linha(); 
  }
  delay(10);
}

void seguirLinha5Canais() {
  // Lendo o estado de todos os 5 sensores
  bool valS1 = digitalRead(S1);
  bool valS2 = digitalRead(S2);
  bool valS3 = digitalRead(S3);
  bool valS4 = digitalRead(S4);
  bool valS5 = digitalRead(S5);

  int velEsq = baseSpeed;
  int velDir = baseSpeed;

  // --- 1. O CENTRO ESTÁ NA LINHA (Situação Ideal) ---
  if (valS3 == LINHA) {
    if (valS2 == LINHA) {
      velEsq = baseSpeed * 0.6; // Corrige pra esquerda
      velDir = baseSpeed;
      mover(velEsq, velDir, 1, 1);
    } 
    else if (valS4 == LINHA) {
      velEsq = baseSpeed;
      velDir = baseSpeed * 0.6; // Corrige pra direita
      mover(velEsq, velDir, 1, 1);
    } 
    else {
      mover(baseSpeed, baseSpeed, 1, 1); // Reto
    }
  }
  // --- 2. PERDEU O CENTRO (Curvas Médias) ---
  else if (valS2 == LINHA && valS3 == FUNDO) {
    velEsq = baseSpeed * 0.2; 
    velDir = baseSpeed;
    mover(velEsq, velDir, 1, 1);
  }
  else if (valS4 == LINHA && valS3 == FUNDO) {
    velEsq = baseSpeed;
    velDir = baseSpeed * 0.2; 
    mover(velEsq, velDir, 1, 1);
  }
  // --- 3. SENSORES EXTREMOS (Curvas Fechadas ou Alinhamento Rápido) ---
  else if (valS1 == LINHA) {
    velEsq = baseSpeed * 0.8; 
    velDir = baseSpeed * 0.8;
    mover(velEsq, velDir, -1, 1); // Rotação no eixo pra esquerda
  }
  else if (valS5 == LINHA) {
    velEsq = baseSpeed * 0.8; 
    velDir = baseSpeed * 0.8;
    mover(velEsq, velDir, 1, -1); // Rotação no eixo pra direita
  }
  // --- 4. TODOS NO FUNDO (Caminhando no vazio) ---
  else if (valS1 == FUNDO && valS2 == FUNDO && valS3 == FUNDO && valS4 == FUNDO && valS5 == FUNDO) {
    // Mantém a última ação até reencontrar a linha
  }
}

// =======================================================
//                MODO 2: DESVIADOR LIVRE
// =======================================================
void modoDesviadorLivre() {
  // 1. PRIORIDADE MÁXIMA: Evitar queda em degraus ou sair da arena (linha)
  bool valS1 = digitalRead(S1);
  bool valS2 = digitalRead(S2);
  bool valS3 = digitalRead(S3);
  bool valS4 = digitalRead(S4);
  bool valS5 = digitalRead(S5);

  // Se qualquer sensor detectar falta de reflexo (buraco ou linha preta)
  if (valS1 == LINHA || valS2 == LINHA || valS3 == LINHA || valS4 == LINHA || valS5 == LINHA) {
    Stop(); delay(100);

    // Dá uma ré rápida para fugir do perigo
    analogWrite(enA, 200);
    analogWrite(enB, 200);
    backword(); delay(300);

    // Fuga inteligente: Se o perigo está na esquerda, foge pra direita e vice-versa
    if (valS1 == LINHA || valS2 == LINHA) {
      turnRight(); delay(400);
    } else {
      turnLeft(); delay(400);
    }
    
    Stop(); delay(100);
    return; // Sai da função para recomeçar o ciclo de leitura seguro
  }

  // 2. NAVEGAÇÃO NORMAL POR ULTRASSOM
  distance_F = Ultrasonic_read();

  // Usa a nova variável com limite maior (ex: 30cm)
  if (distance_F > Set_Livre) {
    analogWrite(enA, baseSpeed);
    analogWrite(enB, baseSpeed);
    forword();
  } else {
    Check_side_livre(); 
  }
  delay(10);
}

// =======================================================
//         FUNÇÕES DE DESVIO PARA MODO LINHA
// =======================================================
void Check_side_linha() {
  Stop(); delay(100);

  for (int angle = 70; angle <= 140; angle += 5) { servoPulse(servo, angle); }
  delay(300);
  distance_R = Ultrasonic_read();
  delay(100);

  for (int angle = 140; angle >= 0; angle -= 5) { servoPulse(servo, angle); }
  delay(500);
  distance_L = Ultrasonic_read();
  delay(100);

  for (int angle = 0; angle <= 70; angle += 5) { servoPulse(servo, angle); }
  delay(300);
  
  compareDistance_linha();
}

void compareDistance_linha() {
  int freioInercia = map(baseSpeed, 70, 200, 80, 250);

  if (distance_L > distance_R) {
    // Desvia pela esquerda
    analogWrite(enA, 200); analogWrite(enB, 200);
    turnLeft();  delay(500);
    forword();   delay(400); // 600
    turnRight(); delay(500);
    forword();   delay(400); // 600
    turnRight(); delay(400); 

    analogWrite(enA, 80); 
    analogWrite(enB, 80);
    forword();            
    
    // Espera QUALQUER um dos 5 sensores encostar na linha
    while (digitalRead(S1) == FUNDO && digitalRead(S2) == FUNDO && digitalRead(S3) == FUNDO && digitalRead(S4) == FUNDO && digitalRead(S5) == FUNDO) { 
      delay(10); 
    }
    
    // Manobra de realinhamento para a esquerda
    analogWrite(enA, 150); analogWrite(enB, 150);
    turnLeft(); 
    delay(200); 
    
    Stop(); delay(freioInercia); 
  }
  else {
    // Desvia pela direita
    analogWrite(enA, 200); analogWrite(enB, 200);
    turnRight(); delay(500);
    forword();   delay(400); // 600
    turnLeft();  delay(500);
    forword();   delay(400); // 600
    turnLeft();  delay(400); 

    analogWrite(enA, 80); 
    analogWrite(enB, 80);
    forword();            
    
    // Espera QUALQUER um dos 5 sensores encostar na linha
    while (digitalRead(S1) == FUNDO && digitalRead(S2) == FUNDO && digitalRead(S3) == FUNDO && digitalRead(S4) == FUNDO && digitalRead(S5) == FUNDO) { 
      delay(10); 
    }
    
    // Manobra de realinhamento para a direita
    analogWrite(enA, 150); analogWrite(enB, 150);
    turnRight(); 
    delay(200); 

    Stop(); delay(freioInercia); 
  }
}

// =======================================================
//         FUNÇÕES DE DESVIO PARA MODO LIVRE
// =======================================================
void Check_side_livre() {
  Stop(); delay(100);

  for (int angle = 70; angle <= 140; angle += 5) { servoPulse(servo, angle); }
  delay(300);
  distance_R = Ultrasonic_read();
  delay(100);

  for (int angle = 140; angle >= 0; angle -= 5) { servoPulse(servo, angle); }
  delay(500);
  distance_L = Ultrasonic_read();
  delay(100);

  for (int angle = 0; angle <= 70; angle += 5) { servoPulse(servo, angle); }
  delay(300);
  
  compareDistance_livre();
}

void compareDistance_livre() {
  analogWrite(enA, 200);
  analogWrite(enB, 200);

  if (distance_L > distance_R) {
    turnLeft(); delay(400); 
  } else {
    turnRight(); delay(400); 
  }
  Stop(); delay(100);
}

// =======================================================
//        FUNÇÕES DE MOTOR, SENSOR E SERVO BÁSICOS
// =======================================================

// Função usada pelo seguidor de 5 canais para movimento proporcional
void mover(int pwmEsq, int pwmDir, int dirEsq, int dirDir) {
  if (dirEsq == 1)      { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); } 
  else if (dirEsq == -1){ digitalWrite(in1, HIGH); digitalWrite(in2, LOW); } 
  else                  { digitalWrite(in1, LOW); digitalWrite(in2, LOW); }
  analogWrite(enA, pwmEsq);

  // CORREÇÃO: dirDir == 1 agora usa HIGH no in3 e LOW no in4, igual ao forword() original
  if (dirDir == 1)      { digitalWrite(in3, HIGH); digitalWrite(in4, LOW); } 
  else if (dirDir == -1){ digitalWrite(in3, LOW); digitalWrite(in4, HIGH); } 
  else                  { digitalWrite(in3, LOW); digitalWrite(in4, LOW); }
  analogWrite(enB, pwmDir);
}

// Funções usadas pela rotina de desvio de obstáculo
void forword()   { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); digitalWrite(in3, HIGH); digitalWrite(in4, LOW); }
void backword()  { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); digitalWrite(in3, LOW); digitalWrite(in4, HIGH); }
void turnRight() { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); digitalWrite(in3, LOW); digitalWrite(in4, HIGH); }
void turnLeft()  { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); digitalWrite(in3, HIGH); digitalWrite(in4, LOW); }
void Stop()      { digitalWrite(in1, LOW); digitalWrite(in2, LOW); digitalWrite(in3, LOW); digitalWrite(in4, LOW); }

void servoPulse(int pin, int angle) {
  int pwm = (angle * 11) + 500;
  digitalWrite(pin, HIGH);
  delayMicroseconds(pwm);
  digitalWrite(pin, LOW);
  delay(50);
}

long Ultrasonic_read() {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  long time = pulseIn(echo, HIGH);
  return time / 29 / 2;
}