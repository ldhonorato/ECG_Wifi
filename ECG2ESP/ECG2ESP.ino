#include <FlexiTimer2.h>
#include <SoftwareSerial.h>
#include <ESP8266.h>

//Coloque nos campos indicados o nome e senha da sua rede WiFi
#define SSID        "Wi-Fi LHRP"
#define PASSWORD    "2806lhss1008rpbn"

// Definições do SHILD ECG
#define NUMCHANNELS 6
#define HEADERLEN 4
#define PACKETLEN (NUMCHANNELS * 2 + HEADERLEN + 1)
#define SAMPFREQ 256                      // ADC sampling rate 256
#define TIMER2VAL (1024/(SAMPFREQ))       // Set 256Hz sampling frequency
#define LED1  13
#define CAL_SIG 9


SoftwareSerial esp8266 (10, 11);
//Cria objeto de conex�o wifi com o m�dulo, usando a Serial1 do Mega.
ESP8266 wifi(esp8266, 19200);
//Variavel para buffer de dados de trasmissao
uint8_t buffer[128] = {0};
//Como usamos multiplas conexoes, cada conexao tem sua ID, e precisa ser armazenada para referencia no programa. Usamos essa variavel para isso.
uint8_t mux_id;
//E esta variavel len serve para armazenar o comprimento de dados recebidos por meio da rotina wifi.recv(), que tambem     //associa ao buffer os dados recebidos e ao mux_id a id responsavel pela transmissao
uint32_t len;
// Global constants and variables
uint8_t TXBuf[PACKETLEN];  //The transmission packet
volatile unsigned char TXIndex;           //Next byte to write in the transmission packet.
volatile unsigned char CurrentCh;         //Current channel being sampled.
volatile unsigned char counter = 0;	  //Additional divider used to generate CAL_SIG
volatile unsigned int ADC_Value = 0;	  //ADC current value

String status;

void Toggle_LED1(void){

	if((digitalRead(LED1))==HIGH){ digitalWrite(LED1,LOW); }
	else{ digitalWrite(LED1,HIGH); }
}

void toggle_GAL_SIG(void){
	
	if(digitalRead(CAL_SIG) == HIGH){ digitalWrite(CAL_SIG, LOW); }
	else{ digitalWrite(CAL_SIG, HIGH); }
	
}

void setup()
{
	//noInterrupts();
	pinMode(LED1, OUTPUT);  //Setup LED1 direction
	digitalWrite(LED1,LOW); //Setup LED1 state
	pinMode(CAL_SIG, OUTPUT);

	//Write packet header and footer
	TXBuf[0] = 0xa5;    //Sync 0
	TXBuf[1] = 0x5a;    //Sync 1
	TXBuf[2] = 2;       //Protocol version
	TXBuf[3] = 0;       //Packet counter
	TXBuf[4] = 0x02;    //CH1 High Byte
	TXBuf[5] = 0x00;    //CH1 Low Byte
	TXBuf[6] = 0x02;    //CH2 High Byte
	TXBuf[7] = 0x00;    //CH2 Low Byte
	TXBuf[8] = 0x02;    //CH3 High Byte
	TXBuf[9] = 0x00;    //CH3 Low Byte
	TXBuf[10] = 0x02;   //CH4 High Byte
	TXBuf[11] = 0x00;   //CH4 Low Byte
	TXBuf[12] = 0x02;   //CH5 High Byte
	TXBuf[13] = 0x00;   //CH5 Low Byte
	TXBuf[14] = 0x02;   //CH6 High Byte
	TXBuf[15] = 0x00;   //CH6 Low Byte
	TXBuf[2 * NUMCHANNELS + HEADERLEN] =  0x01;	// Switches state

  FlexiTimer2::set(TIMER2VAL, Timer2_Overflow_ISR);
  
	Serial.begin(9600);
	
	Serial.println("Iniciando Setup 5.");
	
	Serial.print("Versao de Firmware ESP8266: ");
	//A funcao wifi.getVersion() retorna a versao de firmware informada pelo modulo no inicio da comunicacao
	Serial.println(wifi.getVersion().c_str());
	
	//Vamos setar o modulo para operar em modo Station (conecta em WiFi) e modo AP (� um ponto de WiFi tambem)
	if (wifi.setOprToStationSoftAP())
	{
		Serial.println("Station e AP OK.");
	}
	else
	{
		Serial.println("Erro em setar Station e AP.");
	}
	
	//Agora vamos conectar no ponto de WiFi informado no inicio do codigo, e ver se corre tudo certo
	if (wifi.joinAP(SSID, PASSWORD))
	{
		Serial.println("Conectado com Sucesso.");
		Serial.println("IP: ");
		//rotina wifi.getLocalIP() retorna o IP usado na conexao com AP conectada.
		Serial.println(wifi.getLocalIP().c_str());
	}
	else
	{
		Serial.println("Falha na conexao AP.");
	}
	
	//Agora vamos habiliar a funcionalidade MUX, que permite a realizacao de varias conexoes TCP/UDP
	Serial.println("Indo habilitar mux.");
	if (wifi.enableMUX())
	{
		Serial.println("Conexao mux OK.");
	}
	else
	{
		Serial.println("Erro ao setar mux connection.");
	}
	
	//Inicia servidor TCP na porta 8090 (veja depois a funcao "startServer(numero_porta)", que serve para UDP!
	if (wifi.startTCPServer(8090))
	{
		Serial.println("Servidor iniciado com sucesso.");
	}
	else
	{
		Serial.println("Erro ao iniciar servidor.");
	}
	
  //FlexiTimer2::start();
	Serial.println("Setup finalizado!");
  
}

void Timer2_Overflow_ISR()
{
	// Toggle LED1 with ADC sampling frequency /2
	Toggle_LED1();
	
	//Read the 6 ADC inputs and store current values in Packet
	for(CurrentCh=0;CurrentCh<6;CurrentCh++){
		ADC_Value = analogRead(CurrentCh);
		TXBuf[((2*CurrentCh) + HEADERLEN)] = ((unsigned char)((ADC_Value & 0xFF00) >> 8));	// Write High Byte
		TXBuf[((2*CurrentCh) + HEADERLEN + 1)] = ((unsigned char)(ADC_Value & 0x00FF));	// Write Low Byte
	}
	
	// Send Packet
	status = wifi.getIPStatus();
	if(status.charAt(status.indexOf(':') + 1) == '3')
	{
		//esta conectado
		//if(wifi.send(TXBuf, 17))
		//{
			Serial.print(".");
		//}
		//else
		//{
			//Serial.print("Erro ao enviar\r\n");
		//}

	}
	//for(TXIndex=0;TXIndex<17;TXIndex++){
		//Serial.write(TXBuf[TXIndex]);
	//}
	
	// Increment the packet counter
	TXBuf[3]++;
	
	// Generate the CAL_SIGnal
	counter++;		// increment the devider counter
	if(counter == 12){	// 250/12/2 = 10.4Hz ->Toggle frequency
		counter = 0;
		toggle_GAL_SIG();	// Generate CAL signal with frequ ~10Hz
	}
}


bool flagConectado = false;

void loop()
{
	//__asm__ __volatile__ ("sleep");
	
	if(!flagConectado)
	{
		status = wifi.getIPStatus();
		if(status.charAt(status.indexOf(':') + 1) == '3')
		{
			Serial.println("Conectado");
			flagConectado = true;
		}
	}
	len = wifi.recv(&mux_id, buffer, sizeof(buffer), 100);
	if (len > 0)
	{
		Serial.print("Status:[");
		Serial.print(wifi.getIPStatus().c_str());
		Serial.println("]");
		
		Serial.print("Recebido de :");
		Serial.print(mux_id);
		Serial.print("[");
		for(uint32_t i = 0; i < len; i++)
		{
			Serial.print((char)buffer[i]);
		}
		Serial.print("]\r\n");
		
		//Agora envia de volta. A referencia para o socket TCP criado � o mux_id, ou id da conexao, usado aqui na rotina
		//wifi.send, veja abaixo:
		if(wifi.send(mux_id, buffer, len))
		{
			Serial.print("Enviado de volta...\r\n");
		}
		else
		{
			Serial.print("Erro ao enviar de volta\r\n");
		}
		
		//E, como sempre, liberar a conexao TCP, de modo a permitir que novas conexoes sejam realizadas.
		/*
		if (wifi.releaseTCP(mux_id))
		{
			Serial.print("Liberando conexao TCP com ID: ");
			Serial.print(mux_id);
			Serial.println(" OK");
		}
		else 
		{
			Serial.print("Erro ao liberar TCP com ID: ");
			Serial.print(mux_id);
		}
		
		Serial.print("Status:[");
		Serial.print(wifi.getIPStatus().c_str());
		Serial.println("]");
		*/	
	}
	
}
