
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "auxiliar/display.h"
#include "auxiliar/fontes.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"      
#include "hardware/pwm.h"
#include "locale.h"
#include <string.h>
#include "auxiliar/morse.h"
#include <time.h>

//Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define UART_ID uart0 // Seleciona a UART0.
#define BAUD_RATE 115200 // Define a taxa de transmissão.
#define UART_TX_PIN 0 // Pino GPIO usado para TX.
#define UART_RX_PIN 1 // Pino GPIO usado para RX.
#define ledB_pin 11// led verde GPIO11.
#define ledA_pin 12// led azul GPIO12.
#define ledC_pin 13// led vermelho GPIO13.
#define VRX_PIN 27 
#define VRY_PIN 26 
#define SW_PIN 22 
#define tempo 2500

//Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

//Configurações dos pinos.
const uint button_A = 5; // Botão B = 6.
const uint button_B = 6; // Botão A = 5.
uint32_t current_time;

//Variáveis globais.
static volatile uint a = 1;
static volatile uint b = 1;
static volatile uint32_t last_time = 0; //Armazena o tempo do último evento (em microssegundos).
char apresentar[2];
char caracter;
bool painel = true; 
int k = 0;
int l = 0;
int t = 0;
ssd1306_t ssd; //Inicializa a estrutura do display.
bool cor = true; 
static volatile uint32_t last_time_A = 0; //Armazena o tempo do último evento (em microssegundos).
static volatile uint32_t last_time_SW = 0;//Armazena o tempo do último evento (em microssegundos). 
static volatile bool estado_led3 = false; //Armazena estado do led verde.
char texto[80]; 
char traducao[500];
int tamanhomensagem =0;
volatile bool flag_timer = true;
//char text[80];

char caracter1[5][5]=
{{'y','t', 'o', 'j', 'e' },
{'x', 's', 'n', 'i', 'd'},
{'w', 'r', 'm', 'h', 'c'},
{'v', 'q', 'l', 'g', 'b'},
{'u', 'p', 'k', 'f', 'a'}};

char caracter2[5][5]=
{{'_','_', '.', '7', '2' },
{'_', '_', ',', '6', '1'},
{'_', '_', '0', '5', ' '},
{'_', '?', '9', '4', '_'},
{'_', '!', '8', '3', 'z'}};

//Prototipação da função de interrupção.
static void gpio_irq_handlerA(uint gpio, uint32_t events);

//Definição de PWM. 
#define WRAP 20000 // Para 50Hz (20ms) frequência PWM.
#define Divisor  125  // Divisor de clock ajustado para servo.

//Definição de pixel RGB.
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.


//Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

//Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

static void gpio_irq_handlerA(uint gpio, uint32_t events);

//Função de inicialização para o PWM. 
void pwm_setup()
{
    gpio_set_function(ledC_pin, GPIO_FUNC_PWM); // Configura GPIO 13 como PWM.
    uint slice_red = pwm_gpio_to_slice_num(ledC_pin); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_red, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_red, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_red, true);       // Habilita o PWM.

    gpio_set_function(ledB_pin, GPIO_FUNC_PWM); // Configura GPIO 11 como PWM.
    uint slice_blue = pwm_gpio_to_slice_num(ledB_pin); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_blue, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_blue, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_blue, true);       // Habilita o PWM.
}

//Inicializa a máquina PIO para controle da matriz de LEDs.
void npInit(uint pin) 
{

  //Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  //Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) 
  {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); //Se nenhuma máquina estiver livre, panic!
  }

  //Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  //Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i) 
  {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

//Atribui uma cor RGB a um LED.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) 
{
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
  
//Limpa o buffer de pixels.
void npClear() 
 {
    for (uint i = 0; i < LED_COUNT; ++i)
      npSetLED(i, 0, 0, 0);
}
  
//Escreve os dados do buffer nos LEDs.  
void npWrite() 
 {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) 
    {
      pio_sm_put_blocking(np_pio, sm, leds[i].G);
      pio_sm_put_blocking(np_pio, sm, leds[i].R);
      pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); //Espera 100us, sinal de RESET do datasheet.
}
 
//Relaciona a posição na matriz 5x5 com o incremento 0-24.
int getIndex(int x, int y) 
{
      //Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
      //Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
      if (y % 2 == 0) 
      {
          return y * 5 + x; //Linha par (esquerda para direita).
      } 
      else 
      {
          return y * 5 + (4 - x); //Linha ímpar (direita para esquerda).
      }
}

//Controla a matriz de leds.
void matrizled(bool contador)//Recebe um valor numerico.
{
      npClear();
      switch (painel)
      {
      case true:
         int  tela1[5][5][3] = {//Matriz que representa o número. 
            {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}},
            {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}},
            {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}},
            {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}},
            {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}}};
          for(int linha = 0; linha < 5; linha++)//Percorre as linhas.
          {
            for(int coluna = 0;coluna < 5; coluna++ )//Percorre as colunas.
            {
              int posicao = getIndex(linha, coluna);//Recebe a posição do led.
              npSetLED(posicao, tela1[coluna][linha][0], tela1[coluna][linha][1], tela1[coluna][linha][2]);//Ativa os led de acordo com a cor especificada.
               // tela1[l][k][0] = 255;
                //tela1[l][k][1] = 255;
                //tela1[l][k][2] = 255;
                //tela1[0][0][1] = 255;
                npSetLED(getIndex(k,l), tela1[l][k][0]=255, tela1[l][k][1]=255, tela1[l][k][2]=255);//Ativa os led de acordo com a cor especificada.
            }  
          }
          npWrite();
      break;
      case false:
         int  tela2[5][5][3] = {
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 255, 0}},
            {{0, 255, 0}, {0, 255, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}},
            {{255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}},
            {{255, 0, 0}, {255, 0, 0}, {255, 255, 0}, {0, 0, 0}, {0, 0, 255}}};
          for(int linha = 0; linha < 5; linha++)
          {
            for(int coluna = 0;coluna < 5; coluna++ )
            {
              int posicao = getIndex(linha, coluna);
              npSetLED(posicao, tela2[coluna][linha][0], tela2[coluna][linha][1], tela2[coluna][linha][2]);
                //tela2[l][k][0] = 255;
                //tela2[l][k][1] = 255;
                //tela2[l][k][2] = 255;
                //tela2[0][0][1] = 255;
                //npSetLED(0, tela2[0][0][0], tela2[0][0][1], tela2[0][0][2]);
                npSetLED(getIndex(k,l), tela2[l][k][0]=255, tela2[l][k][1]=255, tela2[l][k][2]=255);//Ativa os led de acordo com a cor especificada.
            }  
          }
          npWrite();
      break;
      }
}

void escrever(int l,int k)
{

  if(painel == true)
  {
    if(gpio_get(SW_PIN)==0)
    {
      texto[t] = caracter1[k][l];
      t++;
    }
  }
  if(painel == false)
  {
    if(gpio_get(SW_PIN)==0)
    {
      if(caracter2[k][l]!='_')
      {
        texto[t] = caracter2[k][l];
        t++;
      }
    }
  }
}  



void delay_ms(int milliseconds) {
  long long start_time = clock() * 1000 / CLOCKS_PER_SEC;
  long long current_times;

  do {
      current_times = clock() * 1000 / CLOCKS_PER_SEC;
      // Aqui você pode executar outras tarefas
  } while (current_times - start_time < milliseconds);
}


void tradutor(char text[80])
{
  int tamanho = strlen(text);
  
  int indice = 0;
  for(int z = 0; z!=tamanho;z++)
    {
      if(text[z] >= '0' && text[z] <= '9')
      {
        indice = (text[z] - '0') ; // Adiciona o deslocamento necessário
      }
    else if ( text[z]>= 'a' && text[z] <= 'z')
      {
        indice = (text[z] - 'a'+ 10);
      }
    else if ( text[z]>= ' ' && text[z]<='?')//&& text[z] <= '?')
      {
        indice =(text[z] - ' ' + 37);
      }
      printf("%d",indice);
      if(indice==37 )
      {
        int final=0;
        final=strlen(traducao);
        traducao[final-1]='\0';
        //strcat(traducao,morse[indice]);
        strcat(traducao," ");
        //traducao[final+1]=' ';
      }
      else
      {
      strcat(traducao,morse[indice]);
      strcat(traducao,"+");
      }
      
    }
    printf("%s",morse[indice]);
    //tamanhomensagem=strlen(traducao);
    int mensagem[500];
   /* for(int w =0; w!=strlen(traducao);w++)
  {
    if(traducao[w]=='.')
    {
      mensagem[w]=40;
    }  
    if(traducao[w]=='-')
    {
      mensagem[w]=120;
    }
    if(traducao[w]==' ')
    {
      mensagem[w]=280;
    }
    if(traducao[w]=='+')
    {
      mensagem[w]=120;
    }
  }*/
  for(int w=0; traducao[w]!='\0'; w++ )
  {
    if(traducao[w]=='.'&&(traducao[w+1]=='.'||traducao[w+1]=='-'))
    {
      gpio_put(ledA_pin,1);
      gpio_put(ledB_pin,1);
      gpio_put(ledC_pin,1);
      printf("ponto continua\n");
      delay_ms(40);
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(40);
      //add_alarm_in_ms(40, repeating_timer_callback_40mS, NULL, false);
    }
    else if(traducao[w]=='.')
    {
      
      gpio_put(ledA_pin,1);
      gpio_put(ledB_pin,1);
      gpio_put(ledC_pin,1);
      delay_ms(40);
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      printf("ponto\n");
    }
    if(traducao[w]=='-'&&(traducao[w+1]=='.'||traducao[w+1]=='-'))
    {
      gpio_put(ledA_pin,1);
      gpio_put(ledB_pin,1);
      gpio_put(ledC_pin,1);
      delay_ms(120);
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(40);
      printf("traco continua\n");
    }
    else if(traducao[w]=='-')
    {
      
      gpio_put(ledA_pin,1);
      gpio_put(ledB_pin,1);
      gpio_put(ledC_pin,1);
      delay_ms(120);
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      printf("traco\n");
    }
    if(traducao[w]==' ')
    {
      
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      printf("espaco\n");
      delay_ms(280);
    }
    if(traducao[w]=='+')
    {
     
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(120);
      printf("letra\n");
    }
    if(traducao[w]=='\0')
    {
      
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      printf("final\n");
    }
  }
  gpio_put(ledA_pin,0);
  gpio_put(ledB_pin,0);
  gpio_put(ledC_pin,0);
  printf("%s\n",traducao);
  /*for(int w = 0; mensagem[w]!='\0';w++)
  {
  printf("%d ",mensagem[w]);
  printf("\n ");
  }*/
  mensagem[0]='\0';
  traducao[0]='\0';
  flag_timer = true;
  //printf("%s\n",traducao);
}

int main()
{
  //Inicializa os periféricos para uso de funções c padrão.
  stdio_init_all();

  setlocale(LC_ALL, "Portuguese");

  //I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  //Inicializa a UART.
  uart_init(UART_ID, BAUD_RATE);

  //Configura os pinos GPIO para a UART.
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); // Configura o pino 0 para TX
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART); // Configura o pino 1 para RX

  //Mensagem inicial.
  const char *init_message = "UART Demo - RP2\r\n"
                              "Digite algo e veja o eco:\r\n";
  uart_puts(UART_ID, init_message);

  //Inicializa o adc.
  adc_init();
  adc_gpio_init(VRX_PIN); 
  adc_gpio_init(VRY_PIN); 

  //Inicializa o gpio para o botão do joystick.
  gpio_init(SW_PIN);
  gpio_set_dir(SW_PIN, GPIO_IN);//Configura o pino como entrada.
  gpio_pull_up(SW_PIN); //Habilita o pull-up interno.

  //Inicializa os leds.
  gpio_init(ledB_pin);
  gpio_set_dir(ledB_pin, GPIO_OUT);
  gpio_init(ledA_pin);
  gpio_set_dir(ledA_pin, GPIO_OUT);
  gpio_init(ledC_pin);
  gpio_set_dir(ledC_pin, GPIO_OUT);

  //Inicializa os botôes.
  gpio_init(button_A);
  gpio_set_dir(button_A, GPIO_IN); //Configura o pino como entrada.
  gpio_pull_up(button_A);          //Habilita o pull-up interno.
  gpio_init(button_B);
  gpio_set_dir(button_B, GPIO_IN); //Configura o pino como entrada.
  gpio_pull_up(button_B);          //Habilita o pull-up interno.

  //Inicializa matriz de LEDs.
  npInit(LED_PIN);
  npClear();
  npWrite(); //Escreve os dados nos LEDs.

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); //Configura o pino GPIO para I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino GPIO para I2C
  gpio_pull_up(I2C_SDA); //Habilita o pull up para os dados
  gpio_pull_up(I2C_SCL); //Habilita o pull up para o clock
  
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); //Inicializa o display.
  ssd1306_config(&ssd); //Configura o display.
  ssd1306_send_data(&ssd); //Envia os dados para o display.

  //Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  //Função de interrupção. 
  gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handlerA);
  gpio_set_irq_enabled(button_B, GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(SW_PIN, GPIO_IRQ_EDGE_FALL, true);


  uart_putc(UART_ID, 1);

  while (true)
  {
    matrizled(painel);
    if(flag_timer){
    //Configura o ADC para os eixos X e Y.
    adc_select_input(0); 
    uint16_t vrx_value = adc_read(); 
    adc_select_input(1); 
    uint16_t vry_value = adc_read(); 
    bool sw_value = gpio_get(SW_PIN) == 0; 

    ssd1306_fill(&ssd, !cor); //Limpa o display.
    ssd1306_rect(&ssd, 3, 3, 120, 56, cor, !cor, 1); //Desenha a borda.
    ssd1306_send_data(&ssd); //Atualiza o display.
    /*if (vrx_value > 2047) 
    {
        Controle_vermelho(vrx_value*5);//Ajusta o brilho.
       sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
    } 
    if((vrx_value < 2047))
    {
        Controle_vermelho((4095-vrx_value)*5);//Ajusta o brilho.
        sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
    }
    if (vry_value > 2047) 
    {
        Controle_azul(vry_value*5);//Ajusta o brilho.
        sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
    } 
    if(vry_value < 2047)
    {
        Controle_azul((4095-vry_value)*5);//Ajusta o brilho.
        sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança 
    }*/
   if(l>=0||l<=4)
   {
    if(vrx_value==0)
    {
      l--;
    } 
    if(vrx_value==4095)
    {
      l++;
    }
   }
   if(l>4){l=0;}
   if(l<0){l=4;}
   if(k>=0||k<=4)
   {
    if(vry_value==0)
    {
      k--;
    } 
    if(vry_value==4095)
    {
      k++;
    }
   }
   if(k>4){k=0;painel=(!painel);}
   if(k<0){k=4;painel=(!painel);}
  
    /*if((vry_value == 2047)&&(vrx_value == 2047))
    {
        
        ssd1306_square(&ssd, 30, 60, 8, 8, cor, cor); //Desenha um quadrado.
        ssd1306_send_data(&ssd); //Atualiza o display.
        sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
    }else
    {
        ssd1306_square(&ssd, (((4095-vrx_value)/79)+2), (((4095-vry_value)/35)+2), 8, 8, cor, cor); //Desenha um retângulo.
        ssd1306_send_data(&ssd); //Atualiza o display.
        sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
    }*/
    printf("VRX: %u, VRY: %u, SW: %d\n", vrx_value, vry_value, sw_value);//imprime valores do ADC.
    printf("l: %d, k: %d\n", l, k);//imprime valores do ADC.
    printf("texto: %s\n", texto);//imprime valores do ADC.
    //printf("texto: %c\n", caracter2[l][k]);//imprime valores do ADC.
    sleep_ms(100);
    //cor = !cor;

    //Lê o caracter inserido. 
    /*if (uart_is_readable(UART_ID)) 
    {
      //Lê um caractere da UART.
      caracter = uart_getc(UART_ID);
      apresentar[0] = caracter; //quarda o caracter lido.
      apresentar[1] = '\0'; 
    
      //Envia de volta o caractere lido (eco).
      uart_putc(UART_ID, caracter);

      //Envia uma mensagem adicional para cada caractere recebido.
      uart_puts(UART_ID, " <- Eco do RP2\r\n");
    }

    //Compara para identificar se foi inserido um caraster numerico.
    if(caracter >= '0' && caracter <= '9')
    {
      matrizled(caracter - '0');//Converte o caracter em um inteiro e aciona a matriz de led.
    }*/
   
    //Atualiza o conteúdo do display com animações.
    ssd1306_fill(&ssd, !cor); //Limpa o display.
    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
    ssd1306_draw_string(&ssd, "digite o codigo", 4, 11); 
    
    ssd1306_draw_string(&ssd, texto, 8, 25); //Desenha uma string.
    ssd1306_hline(&ssd, (t*8)+8, (t*8)+16, 33, 1);
    ssd1306_draw_string(&ssd, "a apaga b envia", 4, 45); 
    if(painel==1)
    {
      char tempStr1[2] = {caracter1[k][l], '\0'};
      ssd1306_draw_string(&ssd, tempStr1, (t*8)+8, 25); 
    }
    else
    {
      char tempStr2[2] = {caracter2[k][l], '\0'};
      ssd1306_draw_string(&ssd, tempStr2, (t*8)+8, 25);
    }
    ssd1306_send_data(&ssd); //Atualiza o display.
    sleep_ms(1000);
  }
}
}

//Função de interrupção com debouncing.
void gpio_irq_handlerA(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos.
    current_time = to_us_since_boot(get_absolute_time());

    //Verifica se passou tempo suficiente desde o último evento.
    //Diferencia o botão A do B.
    if (gpio == button_A && (current_time - last_time > 200000)) //200 ms de debouncing.
    {
        last_time = current_time; //Atualiza o tempo do último evento.
        //printf("Mudanca de Estado do Led. A = %d\n", a);
        /*gpio_put(ledB_pin, !gpio_get(ledB_pin)); //Alterna o estado.
        if(gpio_get(ledB_pin)==false)//Reconhece o estado do led.
        {
          uart_puts(UART_ID, " led azul desligado\r\n");
          ssd1306_fill(&ssd, !cor); //Limpa o display.
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
          ssd1306_draw_string(&ssd, "led azul off", 17, 30);
          ssd1306_send_data(&ssd); //Atualiza o display.
        } else { uart_puts(UART_ID, " led azul ligado\r\n");
          ssd1306_fill(&ssd, !cor); //Limpa o display.
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
          ssd1306_draw_string(&ssd, "led azul on", 17, 30);
          ssd1306_send_data(&ssd); //Atualiza o display.
        }*/
        t--;
        texto[t]=' ';
        a++;//Incrementa a variavel de verificação
    }

    //Diferencia o botão A do B.
    if (gpio == button_B && (current_time - last_time > 200000)) //200 ms de debouncing.
    {
        last_time = current_time; //Atualiza o tempo do último evento.
        //printf("Mudanca de Estado do Led. B = %d\n", b);
        /*gpio_put(ledA_pin, !gpio_get(ledA_pin)); //Alterna o estado.
        if(gpio_get(ledA_pin)==false)//Reconhece o estado do led.
        {
          uart_puts(UART_ID, " led verde desligado\r\n");
          ssd1306_fill(&ssd, !cor); //Limpa o display.
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
          ssd1306_draw_string(&ssd, "led verde off", 14, 30);
          ssd1306_send_data(&ssd); //Atualiza o display.
        } else { uart_puts(UART_ID, " led verde ligado\r\n");
          ssd1306_fill(&ssd, !cor); //Limpa o display.
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
          ssd1306_draw_string(&ssd, "led verde on", 14, 30);
          ssd1306_send_data(&ssd); //Atualiza o display.
        }*/
       ssd1306_fill(&ssd, !cor); //Limpa o display.
       ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
       ssd1306_draw_string(&ssd, " codigo enviado", 4, 11); 
       ssd1306_draw_string(&ssd, texto, 20, 25); //Desenha uma string.
       //ssd1306_draw_string(&ssd, "arepete bvolta", 4, 45); 
       ssd1306_send_data(&ssd); //Atualiza o display.
       gpio_put(ledB_pin,1);
       delay_ms(500);
       gpio_put(ledB_pin,0);
       delay_ms(500);
       tradutor(texto);
       gpio_put(ledC_pin,1);
       delay_ms(500);
       gpio_put(ledC_pin,0);
       delay_ms(500);

       //printf("%s\n",morse[3]);
       //printf("%s\n",text);
        b++;//Incrementa a variavel de verificação.
    }

    if (gpio == SW_PIN && (current_time - last_time > 200000)) //200 ms de debouncing.
    {
        last_time = current_time; //Atualiza o tempo do último evento.
       escrever(l,k);
       printf("texto %s\n",texto);
    }
  
  
}