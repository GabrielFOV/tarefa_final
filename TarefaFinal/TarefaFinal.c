//Codigo criado por Gabriel França Oliveira Viana (TIV370100619)
// Referencias e documentação disponivel em: https://github.com/GabrielFOV/tarefa_final.git

//Na linha 355 é possível definir se é retornado o codigo morse via uart
//Na linha 533 é possível definir se é retornado o texto digitado via uart.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"      
#include "hardware/pwm.h"
#include "auxiliar/display.h"
#include "auxiliar/fontes.h"
#include "auxiliar/morse.h"

//Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14//Conexão com o display.
#define I2C_SCL 15//Conexão com o display.
#define endereco 0x3C
#define UART_ID uart0 // Seleciona a UART0.
#define BAUD_RATE 115200 // Define a taxa de transmissão.
#define UART_TX_PIN 0 // Pino GPIO usado para TX.
#define UART_RX_PIN 1 // Pino GPIO usado para RX.
#define ledB_pin 11// led verde GPIO11.
#define ledA_pin 12// led azul GPIO12.
#define ledC_pin 13// led vermelho GPIO13.
#define VRX_PIN 27//Pino com conexão com o eixo horizontal do joystick. 
#define VRY_PIN 26//Pino com conexão com o eixo vertical do joystick.  
#define SW_PIN 22////Pino com conexão com o botão do joystick.  
#define tempo 2500

//Definição do número de LEDs da matriz e pino.
#define LED_COUNT 25//Quantiadde de leds da matriz. 
#define LED_PIN 7//Pino da matriz led. 

//Configurações dos pinos.
const uint button_A = 5; // Pino do botão A = 5.
const uint button_B = 6; // Pino do botão B = 6.
uint32_t current_time; // Tempo atual para o debouncing.

//Variáveis globais.
static volatile uint a = 1;//contador para o botão a.
static volatile uint b = 1;//contador para o botão b.
static volatile uint32_t last_time = 0; //Armazena o tempo do último evento (em microssegundos).
bool painel = true;//Condição que indica o estado da matriz led. 
int k = 0;//Contador 0 a 4 para a posição da matriz led no eixo vertical. 
int l = 0;//Contador 0 a 4 para a posição da matriz led no eixo horizontal. 
int t = 0;//contador da posição do digito do display.d
ssd1306_t ssd; //Inicializa a estrutura do display.
bool cor = true; 
static volatile uint32_t last_time_A = 0; //Armazena o tempo do último evento (em microssegundos).
static volatile uint32_t last_time_SW = 0;//Armazena o tempo do último evento (em microssegundos). 
char texto[80];//Armazena a mensagem inserida. 
char traducao[500];//Armazena o string convertido em morse.
volatile bool flag_timer = true;// Flag para manter o loop rodando.

char caracter1[5][5]=// matriz correspondente do display led.
{{'y','t', 'o', 'j', 'e' },
{'x', 's', 'n', 'i', 'd'},
{'w', 'r', 'm', 'h', 'c'},
{'v', 'q', 'l', 'g', 'b'},
{'u', 'p', 'k', 'f', 'a'}};

char caracter2[5][5]=// matriz correspondente do display led.
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
  uint offset = pio_add_program(pio0, &ws2818b_program);//Cria programa PIO.
  np_pio = pio0;
  sm = pio_claim_unused_sm(np_pio, false); //Toma posse de uma máquina PIO.
  if (sm < 0) 
  {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); //Se nenhuma máquina estiver livre, panic!
  }
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);  //Inicia programa na máquina PIO obtida.
  for (uint i = 0; i < LED_COUNT; ++i)  //Limpa buffer de pixels.
  {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) ////Atribui uma cor RGB a um LED.
{
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
  
void npClear()////Limpa o buffer de pixels. 
 {
    for (uint i = 0; i < LED_COUNT; ++i)
      npSetLED(i, 0, 0, 0);
}
  
void npWrite() //Escreve os dados do buffer nos LEDs.  
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
 
int getIndex(int x, int y)//Relaciona a posição na matriz 5x5 com o incremento 0-24. 
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
            int  tela1[5][5][3] = {//Matriz que representa o alfabeto. 
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
              npSetLED(getIndex(k,l), tela1[l][k][0]=255, tela1[l][k][1]=255, tela1[l][k][2]=255);//Ativa os led de acordo com a cor especificada.
            }  
          }
          npWrite();
      break;
          case false:
            int  tela2[5][5][3] = {//Matriz que representa os outros caracteres. 
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
              npSetLED(getIndex(k,l), tela2[l][k][0]=255, tela2[l][k][1]=255, tela2[l][k][2]=255);//Ativa os led de acordo com a cor especificada.
            }  
          }
          npWrite();
      break;
      }
}


void escrever(int l,int k)//Função que relaciona a matriz led e a posição do joystick para escrever a string
{

  if(painel == true)//Verifica qual o estado da matriz led. 
  {
    if(gpio_get(SW_PIN)==0)//Verifica se o botão B foi pressionado.
    {
      texto[t] = caracter1[k][l];//a string recebe o caracter equivalente da matriz
      t++;
    }
  }
  if(painel == false)
  {
    if(gpio_get(SW_PIN)==0)
    {
      if(caracter2[k][l]!='_')//Verifica se foi inserido um espaçamento. 
      {
        texto[t] = caracter2[k][l];
        t++;
      }
    }
  }
}  

void delay_ms(int milliseconds) //Adiciona um delay em milisegundos. 
{
  long long start_time = clock() * 1000 / CLOCKS_PER_SEC;//Esta linha obtém o tempo atual usando a função clock(), que retorna o número de ciclos de clock decorridos desde o início do programa.
  long long current_times;
  do //Um loop que continua executando enquanto o tempo decorrido (calculado em milissegundos) for menor que o tempo de atraso desejado
  {
    current_times = clock() * 1000 / CLOCKS_PER_SEC;//Dentro do loop, o tempo atual é obtido novamente e armazenado em current_times.
  } while (current_times - start_time < milliseconds);//Calcula o tempo decorrido desde o início do atraso e compara com o tempo de atraso desejado.
}

void tradutor(char text[80])//Converte a string de caracteres em uma string em morse.
{
  int tamanho = strlen(text);//Armazena o tamanho da string de caracter.
  int indice = 0;// armazena o valor da posição do caracter em relção a posição em fontes.h.
  for(int z = 0; z!=tamanho;z++)//Loop para percorrer toda a string.
    {
      if(text[z] >= '0' && text[z] <= '9')//compara se os caracteres estão entre 0 e 9.
      {
        indice = (text[z] - '0') ; // Adiciona o deslocamento necessário
      }
    else if ( text[z]>= 'a' && text[z] <= 'z')//compara se os caracteres estão entre a e z.
      {
        indice = (text[z] - 'a'+ 10);// Adiciona o deslocamento necessário
      }
    else if ( text[z]>= ' ' && text[z]<='?')//compara se os caracteres estão entre ' ' e ?.
      {
        indice =(text[z] - ' ' + 37);// Adiciona o deslocamento necessário
      }
      if(indice==37 )//Verifica se foi inserido um espaçamento entre os caracteres e adiciona a string em morse.
      {
        int final=0;
        final=strlen(traducao);
        traducao[final-1]='\0';
        strcat(traducao," ");
      }
      else//Adiciona um + ao final de cada caracter em morse, para incdicar o fim do caracter.
      {
        strcat(traducao,morse[indice]);
        strcat(traducao,"+");
      }
    }
    //printf("%s",morse[indice]);//imprime a mensagem digitada em codigo morse usando ., -, ' ' e +. 
  for(int w=0; traducao[w]!='\0'; w++ )//percorre o string em morse para acionar o led pelo tempo de acordo com o codigo morse.
  {
    if(traducao[w]=='.'&&(traducao[w+1]=='.'||traducao[w+1]=='-'))
    {
      gpio_put(ledA_pin,1);
      gpio_put(ledB_pin,1);
      gpio_put(ledC_pin,1);
      delay_ms(40);
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(40);
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
    }
    if(traducao[w]==' ')
    {
      
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(280);
    }
    if(traducao[w]=='+')
    {
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
      delay_ms(120);
    }
    if(traducao[w]=='\0')
    {
      gpio_put(ledA_pin,0);
      gpio_put(ledB_pin,0);
      gpio_put(ledC_pin,0);
    }
  }
  //Garante o desligamento dos leds ao final da transmissão.
  gpio_put(ledA_pin,0);
  gpio_put(ledB_pin,0);
  gpio_put(ledC_pin,0);
  //printf("%s\n",traducao);//habilita a impressão do codigo morse equivalente na uART.
  traducao[0]='\0';// "Limpa o armazenamento" da string em morse.
  flag_timer = true;//Garante o retorno do loop principal.
}

int main()
{
  //Inicializa os periféricos para uso de funções c padrão.
  stdio_init_all();

  //I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  //Inicializa a UART.
  uart_init(UART_ID, BAUD_RATE);

  //Configura os pinos GPIO para a UART.
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); // Configura o pino 0 para TX
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART); // Configura o pino 1 para RX

  //Mensagem inicial.
  /*const char *init_message = "UART Demo - RP2\r\n"
                              "Digite algo e veja o eco:\r\n";
  uart_puts(UART_ID, init_message);*/

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
  gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handlerA);//Botão A.
  gpio_set_irq_enabled(button_B, GPIO_IRQ_EDGE_FALL, true);//Botão B
  gpio_set_irq_enabled(SW_PIN, GPIO_IRQ_EDGE_FALL, true);//Botão do joystick.

  uart_putc(UART_ID, 1);//Ativa a uart.

  while (true)
  {
    matrizled(painel);//Ativa a matriz led no painel de leds
    if(flag_timer)//Flag para a continuidade do loop.
    {
    //Configura o ADC para os eixos X e Y respectivamente.
    adc_select_input(0); 
    uint16_t vrx_value = adc_read(); 
    adc_select_input(1); 
    uint16_t vry_value = adc_read(); 
    bool sw_value = gpio_get(SW_PIN) == 0;//Identifica o acionamento do botão do joystick. 

    ssd1306_fill(&ssd, !cor); //Limpa o display.
    ssd1306_rect(&ssd, 3, 3, 120, 56, cor, !cor, 1); //Desenha a borda.
    ssd1306_send_data(&ssd); //Atualiza o display.
   if(l>=0||l<=4)//Identifica o movimento do joystick na horizontal.
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
   if(l>4){l=0;}//condiciona a reconhecer a posição do joystick como o oposto apos passar de certo ponto.
   if(l<0){l=4;}//condiciona a reconhecer a posição do joystick como o oposto apos passar de certo ponto.
   if(k>=0||k<=4)//Identifica o movimento do joystick na vertical.
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
   if(k>4){k=0;painel=(!painel);}//condiciona a reconhecer a posição do joystick como o oposto apos passar de certo ponto e muda a matriz led.
   if(k<0){k=4;painel=(!painel);}//condiciona a reconhecer a posição do joystick como o oposto apos passar de certo ponto e muda a matriz led. 
    //Atualiza o conteúdo do display com animações.
    ssd1306_fill(&ssd, !cor); //Limpa o display.
    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
    ssd1306_draw_string(&ssd, "digite o codigo", 4, 11); 
    ssd1306_draw_string(&ssd, texto, 8, 25); //Desenha uma string.
    ssd1306_hline(&ssd, (t*8)+8, (t*8)+16, 33, 1);//Desenha uma linha sobre o caracter que está selecionado de acordo com o display.
    ssd1306_draw_string(&ssd, "a apaga b envia", 4, 45); 
    if(painel==1)//Escreve no display o caracter em que o joystick está indicando no painel 1.
    {
      char tempStr1[2] = {caracter1[k][l], '\0'};
      ssd1306_draw_string(&ssd, tempStr1, (t*8)+8, 25); 
    }
    else//Escreve no display o caracter em que o joystick está indicando no painel 2.
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
    //Diferencia o botão pressionado.
    if (gpio == button_A && (current_time - last_time > 200000)) //200 ms de debouncing.
    {//Quando pressionado o botão A, o ultimo caracter inserido é apagado. 
        last_time = current_time; //Atualiza o tempo do último evento.
        t--;//decrementa a posição do caracter no display
        texto[t]=' ';
    }
    if (gpio == button_B && (current_time - last_time > 200000)) //200 ms de debouncing.
    {//Quando pressionado o botão B a mensagem é enviada para ser convertida em codigo morse.
        last_time = current_time; //Atualiza o tempo do último evento.
      
       //modifica a tela apresentada no display. 
       ssd1306_fill(&ssd, !cor); //Limpa o display.
       ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor, 1); //Desenha um retângulo.
       ssd1306_draw_string(&ssd, " codigo enviado", 4, 11); 
       ssd1306_draw_string(&ssd, texto, 20, 25); //Desenha uma string.
       //ssd1306_draw_string(&ssd, "arepete bvolta", 4, 45); 
       ssd1306_send_data(&ssd); //Atualiza o display.
       gpio_put(ledB_pin,1);//Acende um led azul, indicando o inicio do codigo.
       delay_ms(500);
       gpio_put(ledB_pin,0);//Desliga o led.
       delay_ms(500);
       tradutor(texto);//pisca o led com uma luz branca para transmitir o codigo.
       gpio_put(ledC_pin,1);//Acende um led vermelho, indicando o fim do codigo.
       delay_ms(500);
       gpio_put(ledC_pin,0);//Desliga o led.
       delay_ms(500);
    }
    if (gpio == SW_PIN && (current_time - last_time > 200000)) //200 ms de debouncing.
    {//Quando pressionado o botão do joystick o caracter correspondente é adicionado a string. 
      last_time = current_time; //Atualiza o tempo do último evento.
       escrever(l,k);//Chama a função para obter o caracter equivalente.
       //printf("texto %s\n",texto);//Habilita a impressão do texto inserido na uart. 
    }
}