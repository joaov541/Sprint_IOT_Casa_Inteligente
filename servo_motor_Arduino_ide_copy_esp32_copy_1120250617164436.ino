#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>

// --- WiFi & MQTT ---
const char* ssid = "ProjetosIoT_Esp32";//sua rede wifi
const char* password = "senai@134";//senha da sua rede wifi
const char* mqtt_server = "broker.hivemq.com";//endereço do broker público
const int mqtt_port = 1883;//porta do broker público, geralmente 1883

//Tópicos
const char* topic_led = "teacheagles/lab19/luzsala";
const char* topic_temp = "teacheagles/lab19/temperatura";
const char* topic_umid = "teacheagles/lab19/umidade";
const char* topic_porta = "teacheagles/sala/porta";

// --- Pinos ---
const int luzSala = 13;
const int rele = 15;
const int servoMotor = 19;
const int trigPin = 5;   // Pino Trigger
const int echoPin = 21;  // Pino Echo
const int distanciaLimite = 10;

long duracao;     // Variável para armazenar o tempo do eco (em microssegundos)
int distanciaCm;  // Variável para armazenar a distância calculada (em cm)

#define DHTPIN 33
#define DHTTYPE DHT11

// --- Objetos ---
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);
Servo motor;

// --- Variáveis ---
int contadorGas = 0;
unsigned long ultimaLeitura = 0;

// --- Funções WiFi e MQTT ---
void conectarWiFi() {//verifica conexão wifi para somente depois iniciar o sistema
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

void reconectarMQTT() {//verifica e reconecta a conexão com o broker mqtt
  while (!client.connected()) {
    Serial.print("Reconectando MQTT...");
    if (client.connect("ESP32ClientTest")) {
      Serial.println("Conectado!");
      client.subscribe(topic_led);//conecta ao topico do led assim que estabelecer ligação com o broker
      client.subscribe(topic_porta);//conecta ao topico da porta assim que estabelecer ligação com o broker
    } else {
      Serial.print("Falha: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

/**
  Função para tratamento das mensagens de callback/retorno do broker de cada tópico subscrito (led, porta, etc.)

  char* é uma cadeia de caracteres em C como um vetor onde cada caractter/letra está em uma posição, 
  diferente de uma String em C++ que pode ser lida completamente
*/
void tratarMensagem(char* topic, byte* payload, unsigned int length) {//
  String mensagem = "";
  for (int i = 0; i < length; i++) {//concatena todas os char* para se ter o texto completo em String
    mensagem += (char)payload[i];
  }

  Serial.printf("Mensagem recebida [%s]: %s\n", topic, mensagem.c_str());
  
  //led - luz da sala
  if (strcmp(topic, topic_led) == 0) {//tópico atual é o do led?
    if (mensagem == "ligar") {
      digitalWrite(luzSala, HIGH);
    } else if (mensagem == "desligar") {
      digitalWrite(luzSala, LOW);
    }
  }
  
  /*
    Verifica se o tópico recebido é o topico da porta
  é uma função da linguagem C que compara duas strings (topic e topic_porta)
  */
  //porta
  if (strcmp(topic, topic_porta) == 0) {//tópico atual é o da porta?
    if (mensagem == "abrir") {
      destrancarPorta();
      delay(500);
      abrirPortaAutomatico();
    } else if (mensagem == "fechar") {
      fecharPortaAutomatico();
      delay(500);
      trancarPorta();
    }
  }
}

// --- Sensores e atuadores ---

void lerSensorEDisponibilizar() {
  float temperatura = dht.readTemperature();//lê a temperatura do sensor
  float umidade = dht.readHumidity();// lê a umidade do sensor

  if (isnan(temperatura) || isnan(umidade)) {//avisa no console se deu erro
    Serial.println("Erro ao ler DHT!");
    return;
  }

  Serial.printf("Temp: %.1f °C | Umid: %.1f %%\n", temperatura, umidade);//mostra temperatura e umidade no console

  char tempStr[10], umidStr[10];
  dtostrf(temperatura, 4, 1, tempStr); //converte o valor da temperatura do sensor que para string (ele vem float do sensor)
  dtostrf(umidade, 4, 1, umidStr); //converte o valor da umidade do sensor que para string (ele vem float do sensor)
  client.publish(topic_temp, tempStr);//publica a temperatura no tópico, lá no Broker Server
  client.publish(topic_umid, umidStr);//publica a umidade no tópico, lá no Broker Server
}

void acenderLedMovimento() {

  digitalWrite(trigPin, LOW);  //Resentando o estado desse pin, para garantir que está no estado inicial
  delayMicroseconds(10);       //Determina um tempo


  digitalWrite(trigPin, HIGH);  //Mudando o estado deste pin para Alto, para começar a enviar onda sonora
  delayMicroseconds(10);        // Posto um tempo pois o HC-SR04 precisa de pelo menos 10µs para que receba o pulso corretamente

  digitalWrite(trigPin, LOW);


  duracao = pulseIn(echoPin, HIGH);  // Mede o tempo (em microssegundos) que o pino Echo ficou em HIGH e guarda em 'duracao'.

  // Calcula a distância em centímetros
  distanciaCm = (duracao * 0.034) / 2;  // Converte tempo em distância (cm)


  if (distanciaCm > 0 && distanciaCm <= distanciaLimite) {
    digitalWrite(luzSala, HIGH);
    Serial.println("Algo está se aproximando, está há:" + String(distanciaCm) + "cm");

  } else {
    digitalWrite(luzSala, LOW);
    Serial.println("Nenhum objeto no campo de " + String(distanciaLimite) + "cm");
  }

  //Atribuido um tempo para durar um o comportamente instruino na condição acima
  delay(500);
}



void destrancarPorta() {//trava elétrica 12v
  digitalWrite(rele, HIGH);
  Serial.println("Porta destrancada");
}

void trancarPorta() {//trava elétrica 12v
  digitalWrite(rele, LOW);
  Serial.println("Porta trancada");
}

void abrirPortaAutomatico() {
  motor.write(180);
  Serial.println("Porta aberta");
}

void fecharPortaAutomatico() {
  motor.write(0);
  Serial.println("Porta fechada");
}

// --- Setup ---
void setup() {//configuração inicial dos sensores
  Serial.begin(115200);//inicia a serial do esp32 - ATENÇÃO: tem que colocar a serial do Arduino IDE na mesma velocidade

  //configura os pinos dos sensores
  pinMode(luzSala, OUTPUT);//Inicia o led como saída
  pinMode(rele, OUTPUT);//inicia o sensor relê como saída para controle da trava elétrica
  digitalWrite(rele, LOW);//já fecha a porta através do sensor relê

  motor.attach(servoMotor);//inicia o servo motor (interamente é como se ele desse o comando pinMode() pela biblioteca do Servo)
  motor.write(160);//porta fechada está em 180 grau do motor !!!

  dht.begin();//inicia o sensor dht
  conectarWiFi();//conecta no wifi
  client.setServer(mqtt_server, mqtt_port);//conecta no broker server
  client.setCallback(tratarMensagem);//trata as mensagens recebidas do broker

  Serial.println("Sistema iniciado!");
}

// --- Loop ---
void loop() {
  if (!client.connected()) reconectarMQTT();//se não tem conexão com o broker, tenta reconectar
  client.loop(); //mantém a conexão com o broker serve sempre aberta

  acenderLedMovimento();
  destrancarPorta();
  trancarPorta();
  abrirPortaAutomatico();
  fecharPortaAutomatico(); 
  //acenderLEDAoDetectarPresenca();
  //verificarVazamentoDeGas();

}