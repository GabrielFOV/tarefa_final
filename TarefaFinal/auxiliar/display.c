#include "display.h"
#include "fontes.h"

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
  ssd->width = width;//Armazena a largura do display na estrutura.
  ssd->height = height;//Armazena a altura do display na estrutura.
  ssd->pages = height / 8U;//Calcula o número de páginas (8 pixels por página) e
  ssd->address = address;//Armazena o endereço I2C do display na estrutura.
  ssd->i2c_port = i2c;//Armazena o ponteiro para a instância do controlador I2C na estrutura.
  ssd->bufsize = ssd->pages * ssd->width + 1;//Calcula o tamanho do buffer de RAM necessário
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));//Aloca memória para o buffer de RAM usando calloc, que inicializa todos os bytes com zero.
  ssd->ram_buffer[0] = 0x40;//Define o primeiro byte do buffer de RAM como 0x40, que é o byte de controle para dados de exibição.
  ssd->port_buffer[0] = 0x80;//Define o primeiro byte do buffer de comando como 0x80, que é o byte de controle para comandos.
}

void ssd1306_config(ssd1306_t *ssd)//A função envia uma série de comandos para o display OLED usando a função ssd1306_command. 
{//Configura e inicializa o display OLED SSD1306 com os parâmetros necessários para o funcionamento correto.
  ssd1306_command(ssd, SET_DISP | 0x00);//Desliga o display.
  ssd1306_command(ssd, SET_MEM_ADDR);//Define o modo de endereçamento da memória.
  ssd1306_command(ssd, 0x01);//Configura o modo de endereçamento para horizontal.
  ssd1306_command(ssd, SET_DISP_START_LINE | 0x00);//Define a linha inicial de exibição.
  ssd1306_command(ssd, SET_SEG_REMAP | 0x01);//Remapeia os segmentos (colunas) do display.
  ssd1306_command(ssd, SET_MUX_RATIO);//Define a razão de multiplexação.
  ssd1306_command(ssd, HEIGHT - 1);//Define a altura do display (HEIGHT - 1).
  ssd1306_command(ssd, SET_COM_OUT_DIR | 0x08);//Define a direção de saída COM (linhas).
  ssd1306_command(ssd, SET_DISP_OFFSET);//Define o deslocamento de exibição.
  ssd1306_command(ssd, 0x00);//Define o deslocamento de exibição para 0.
  ssd1306_command(ssd, SET_COM_PIN_CFG);//Define a configuração dos pinos COM.
  ssd1306_command(ssd, 0x12);//Configura os pinos COM para um layout específico.
  ssd1306_command(ssd, SET_DISP_CLK_DIV);//Define a divisão do clock de exibição.
  ssd1306_command(ssd, 0x80);//Define a divisão do clock de exibição e a frequência do oscilador.
  ssd1306_command(ssd, SET_PRECHARGE);//Define o período de pré-carga.
  ssd1306_command(ssd, 0xF1);//Define o período de pré-carga para um valor específico.
  ssd1306_command(ssd, SET_VCOM_DESEL);//Define o nível de deseleção VCOM.
  ssd1306_command(ssd, 0x30);//Define o nível de deseleção VCOM para um valor específico.
  ssd1306_command(ssd, SET_CONTRAST);//Define o contraste do display.
  ssd1306_command(ssd, 0xFF);//Define o contraste para o valor máximo.
  ssd1306_command(ssd, SET_ENTIRE_ON);//Desativa a exibição de toda a tela ligada.
  ssd1306_command(ssd, SET_NORM_INV);//Define a exibição normal (não invertida).
  ssd1306_command(ssd, SET_CHARGE_PUMP);//Define a configuração da bomba de carga.
  ssd1306_command(ssd, 0x14);//Ativa a bomba de carga.
  ssd1306_command(ssd, SET_DISP | 0x01);//Liga o display (SET_DISP com o bit 0 ligado).
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command) //Envia os dados para o display via I2C.
{
  ssd->port_buffer[1] = command;// Armazena o comando no segundo byte.
  i2c_write_blocking//Chama uma função para enviar os dados através do protocolo I2C.
  (
    ssd->i2c_port,//A porta a ser usada.
    ssd->address,//o endereço do display.
    ssd->port_buffer,//O buffer de dados a ser enviado.
    2,//numero de bytes a ser enviado
    false//Não há um stop no final da transmissão. 
  );
}

void ssd1306_send_data(ssd1306_t *ssd) //Envia os dados do buffer de RAM para o display. 
{
  ssd1306_command(ssd, SET_COL_ADDR);//Envia o comando SET_COL_ADDR para o display, iniciando a configuração do endereço da coluna.
  ssd1306_command(ssd, 0);//Define o endereço da coluna inicial como 0.
  ssd1306_command(ssd, ssd->width - 1);//Define o endereço da coluna final como o último pixel da largura do display.
  ssd1306_command(ssd, SET_PAGE_ADDR);//Envia o comando SET_PAGE_ADDR para o display, iniciando a configuração do endereço da página.
  ssd1306_command(ssd, 0);//Define o endereço da linha inicial como 0.
  ssd1306_command(ssd, ssd->pages - 1);//Define o endereço da linha final como a última linha do display.
  i2c_write_blocking
  (
    ssd->i2c_port,//A porta a ser usada.
    ssd->address,//o endereço do display.
    ssd->ram_buffer,//O buffer contendo os dados a serem enviados.
    ssd->bufsize,//O tamanho do buffer de dados.
    false//Não há um stop no final da transmissão. 
  );
}

void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value)//Controla o estado de um pixel individual no buffer de RAM. 
{
  uint16_t index = (y >> 3) + (x << 3) + 1;//Esta linha calcula o índice no buffer  onde o pixel está armazenado.
  uint8_t pixel = (y & 0b111);//Calcula a posição do bit dentro do byte onde o pixel está armazenado.
  if (value)
    ssd->ram_buffer[index] |= (1 << pixel);//Acende. Define o bit correspondente ao pixel como 1 usando uma operação OR bit a bit.
  else
    ssd->ram_buffer[index] &= ~(1 << pixel);//Apaga. Define o bit correspondente ao pixel como 0 usando uma operação AND bit a bit 
}

void ssd1306_fill(ssd1306_t *ssd, bool value)//Deixar todos os leds do display no estado ligado ou desligado. 
{
    // Itera por todas as posições do display
    for (uint8_t y = 0; y < ssd->height; ++y)//Itera por todas as linhas (coordenadas Y) do display. 
    {
        for (uint8_t x = 0; x < ssd->width; ++x)//Itera por todas as colunas (coordenadas X) do display.
        {
            ssd1306_pixel(ssd, x, y, value);//liga/desliga os leds.
        }
    }
}

//Desenhar um retângulo(bordas) no display.
void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill, uint8_t thickness) 
{
  for (uint8_t t = 0; t < thickness; ++t)//Controla a espessura da borda, desenhando linhas paralelas. 
  {
    for (uint8_t x = left; x < left + width; ++x)//Itera pelas colunas dentro da largura do retângulo. 
    {
      ssd1306_pixel(ssd, x, top + t, value);//Desenha os pixels na linha superior.
      ssd1306_pixel(ssd, x, top + height - 1 - t, value);//Desenha os pixels na linha inferiror.
    }
  }
  for (uint8_t t = 0; t < thickness; ++t) 
  {
    for (uint8_t y = top; y < top + height; ++y)//Desenha as linhas verticais. 
    {
      ssd1306_pixel(ssd, left + t, y, value);//Desenha os pixels nas linhas esquerda.
      ssd1306_pixel(ssd, left + width - 1 - t, y, value);//Desenha os pixels na linha direita.
    }
  }
  
  if (fill)//Se o parâmetro fill==true, o retângulo será preenchido. 
  {
    for (uint8_t x = left + 1; x < left + width - 1; ++x) 
    {
      for (uint8_t y = top + 1; y < top + height - 1; ++y) 
      {
        ssd1306_pixel(ssd, x, y, value);
      }
    }
  }
}
  
//Desenhar uma linha reta no display.
void ssd1306_line(ssd1306_t *ssd, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value)
{
    int dx = abs(x1 - x0);//Calcula a diferença absoluta entre as coordenadas X dos pontos inicial e final.
    int dy = abs(y1 - y0);//Calcula a diferença absoluta entre as coordenadas Y dos pontos inicial e final.
    int sx = (x0 < x1) ? 1 : -1;//Determina a direção do incremento em X (1 para direita, -1 para esquerda).
    int sy = (y0 < y1) ? 1 : -1;//Determina a direção do incremento em Y (1 para baixo, -1 para cima).
    int err = dx - dy;//Inicializa a variável de erro, que é usada para determinar qual pixel desenhar.
    while (true) 
    {
        ssd1306_pixel(ssd, x0, y0, value); // Desenha o pixel atual nas coordenadas (x0, y0).
        if (x0 == x1 && y0 == y1) break; // Verifica se o ponto atual é o ponto final; se for, o loop termina.
        int e2 = err * 2;//Calcula o dobro da variável de erro.
        if (e2 > -dy) //Verifica se a decisão de incrementar X deve ser tomada.
        {
          err -= dy;//Atualiza a variável de erro.
          x0 += sx;//Incrementa a coordenada X na direção correta.
        }
        if (e2 < dx)//Verifica se a decisão de incrementar Y deve ser tomada. 
        {
          err += dx;//Atualiza a variável de erro.
          y0 += sy;//Incrementa a coordenada Y na direção correta.
        }
    }
}

//Desenha uma linha horizontal no display.
void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value) 
{
  for (uint8_t x = x0; x <= x1; ++x)//Um loop que itera por todas as coordenadas X.
    ssd1306_pixel(ssd, x, y, value);//Dentro do loop, a função ssd1306_pixel é chamada para definir o estado de cada pixel na linha horizontal.
}

//Desenha uma linha vertical no display. 
void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value) 
{
  for (uint8_t y = y0; y <= y1; ++y)//Um loop que itera por todas as coordenadas Y.
    ssd1306_pixel(ssd, x, y, value);//Dentro do loop, a função ssd1306_pixel é chamada para definir o estado de cada pixel na linha vertical.
}

// Função para desenhar um caractere
void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y)
{//Determina o índice correto no array font onde os dados do caractere estão armazenados.
  uint16_t index = 0;
  if (c >= 'A' && c <= 'Z')//Verificando o intervalo do caracterepara letras maiúsculas.
  {
    index = (c - 'A' + 11) * 8; // Para letras maiúsculas.
  }else  if (c >= '0' && c <= '9')//Verificando o intervalo do caractere para números.
  {
    index = (c - '0' + 1) * 8; // Adiciona o deslocamento necessário.
  }else if ( c>= 'a' && c <= 'z')//Verificando o intervalo do caractere para letras minúsculas.
  {
    index = (c - 'a'+ 37) * 8;//Adiciona o deslocamento necessário.
  }else if ( c>= ' ' && c <= '?')//Verificando o intervalo do caractere para simbolos.
  {
    index = (c - ' '+ 63) * 8;//Adiciona o deslocamento necessário.
  }
  for (uint8_t i = 0; i < 8; ++i)//Um loop externo que itera pelos 8 bytes de dados do caractere.
  {
    uint8_t line = font[index + i];//Obtém o byte de dados correspondente à coluna atual do caractere.
    for (uint8_t j = 0; j < 8; ++j)//Um loop interno que itera pelos 8 bits dentro do byte de dados (representando as 8 linhas do caractere).
    {
      ssd1306_pixel(ssd, x + i, y + j, line & (1 << j));//Define o estado do led no display.
    }
  }
}

// Função para desenhar uma string
void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y)
{
  while (*str)//Um loop que continua enquanto o caractere apontado por str não for o caractere nulo ('\0')
  {
    ssd1306_draw_char(ssd, *str++, x, y);//Chama a função ssd1306_draw_char para desenhar o caractere atual (*str).
    x += 8;//Incrementa a coordenada X em 8 pixels (a largura de um caractere) para posicionar o próximo caractere.
    if (x + 8 >= ssd->width)//Verifica se o próximo caractere ultrapassaria a largura do display.
    {
      x = 0;//Se ultrapassar, redefine a coordenada X para 0 (início da próxima linha).
      y += 8;//Incrementa a coordenada Y em 8 pixels (a altura de um caractere) para iniciar a próxima linha.
    }
    if (y + 8 >= ssd->height)//Verifica se a próxima linha ultrapassaria a altura do display.
    {
      break;//Se ultrapassar, o loop termina (a string é truncada).
    }
  }
}