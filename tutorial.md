# Trabalho 1 — Controle de Estacionamentos

Trabalho 1 da disciplina de **Fundamentos de Sistemas Embarcados (2025/2)**

## 1. Objetivos

Criar um **sistema distribuído** para controle e monitoramento de um estacionamento de 3 andares, composto por:

* **Entrada/saída de veículos** com cancelas e sensores.
* **Leitura de ocupação** de vagas por endereço (multiplexação GPIO).
* **Cobrança por tempo de permanência** (valor/minuto).
* **Integração serial RS485‑MODBUS** com:

  1. **Câmera LPR (*License Plate Recognition*) da entrada** captura de placa no instante da chegada.
  2. **Câmera LPR da saída** (captura de placa no instante da saída para cálculo do valor).
  3. **Placar de vagas** (dispositivo de placar no barramento que exibe vagas livres por andar e total de vagas por tipo)

A arquitetura contém um **Servidor Central** e **Servidores Distribuídos** (um por andar), todos sendo executados na placa Raspberry Pi. A comunicação entre servidores é realizada via **TCP/IP** (JSON sugerido).

O servidor do **Andar Térreo** tem ainda um módulo de comunicação serial via **barramento RS485‑MODBUS RTU compartilhado** para se comunicar com a câmera de entrada, a câmera de saída e o placar (cada um com **endereço MODBUS** distinto).

Cancela | Sinalização de Vagas  |  Panel Geral
:-------------------------:|:-------------------------:|:-------------------------:
 <img src="https://www.apontorapido.com.br/imagens/informacoes/comprar-cancela-estacionamento-01.webp" width="200"/> | <img src="https://seegabc.org.br/wp-seeg/uploads/2017/09/O_crescimento_da_tecnologia.jpg" width="200"/> | <img src="https://yata-apix-3b3e3703-30c5-4a9c-ade8-096ac39ff17c.s3-object.locaweb.com.br/d5b1c273761f481180935c6fb78da8fd.jpg" width="200"/>

Câmera LDR

<img src="figuras/Camera_LPR.jpg" width="600"/>

## 2. Arquitetura do Sistema

### 2.1 Servidor Central

  Responsável por:

  * consolidar ocupação por andar/estacionamento;  
  * persistir eventos (entradas, saídas, placas, timestamps);  
  * **calcular o valor a pagar**;  
  * prover **interface** (terminal) para operação e comandos (fechar estacionamento, bloquear andares, etc.).

### 2.2 Servidor Distribuído Andar Térreo

  Responsável por:

  * ler sensores de vaga (varredura por endereço 0–7);
  * publicar estatísticas no **placar MODBUS**;
  * publicar os eventos (entradas e saídas de veículos, etc) para o servidor central (TCP/IP)
  * controlar cancelas de entrada e saída;
  * **interagir com os dispositivos MODBUS** conforme a função (ver §4):

    * **Entrada**: quando um veículo chega e aciona o sensor, **disparar a leitura MODBUS** na câmera de entrada para obter **placa**;
    * **Saída**: quando um veículo chega à cancela de saída, **disparar a leitura MODBUS** na câmera de saída para obter **placa** e então solicitar ao Servidor Central o **cálculo do custo**;
    * **Placar**: sob comando do Servidor Central, **escrever** as quantidades de vagas livres (andar 1, andar 2, total).

Diagrama do servidor Térreo

<img src="figuras/Diagrama_Servidor_Estacionamento_Terreo.PNG" width="600"/>

### 2.3 Servidores Distribuídos 1º e 2º andares

  Responsáveis por:

  * ler sensores de vaga (varredura por endereço 0–7);
  * detectar passagem entre andares no **1º e 2º andares** (sensor 1 → 2 sobe, 2 → 1 desce);
  * publicar os eventos (entradas e saídas de veículos, etc) para o servidor central (TCP/IP).

Diagrama do servidor (Andares superiores)

<img src="figuras/Diagrama_Servidor_Estacionamento_Andares.PNG" width="600"/>

### 2.4 Barramento Serial RS485‑MODBUS RTU

  * Físico: RS485 half‑duplex, terminadores e bias.
  * **/dev/ttyUSB0** (ou conforme a interface) em cada Raspberry Pi que interage com o barramento.
  * Parâmetros padrão: **B115200 8N1**, timeout 200–500 ms, até 3 tentativas em caso de erro.

> **Orientação**: centralizar a interface MODBUS no servidor distribuído do 1º andar.

## 3. GPIO e Topologia dos Andares

Atribuições de GPIO para cada andar estão definidas nas Tabelas 1 a 3:

<center>
<b>Tabela 1</b> - Pinout da GPIO da Raspberry Pi do <b>Andar Térreo</b>
</center>
<center> 

| Item                                              | GPIO | Direção |
|---------------------------------------------------|:----:|:-------:|
| ENDERECO_01                                       |  17  | Saída   |
| ENDERECO_02                                       |  18  | Saída   |
<!-- | ENDERECO_03                                       |  23  | Saída   | -->
| SENSOR_DE_VAGA                                    |  08  | Entrada |
| SENSOR_ABERTURA_CANCELA_ENTRADA                   |  07  | Entrada |
| SENSOR_FECHAMENTO_CANCELA_ENTRADA                 |  01  | Entrada |
| MOTOR_CANCELA_ENTRADA                             |  23  | Saída   |
| SENSOR_ABERTURA_CANCELA_SAIDA                     |  12  | Entrada |
| SENSOR_FECHAMENTO_CANCELA_SAIDA                   |  25  | Entrada |
| MOTOR_CANCELA_SAIDA                               |  24  | Saída   |

</center> 

<center>
<b>Tabela 2</b> - Pinout da GPIO da Raspberry Pi do <b>1º Andar</b>
</center>
<center> 

| Item                                              | GPIO | Direção |
|---------------------------------------------------|:----:|:-------:|
| ENDERECO_01                                       |  16  | Saída   |
| ENDERECO_02                                       |  20  | Saída   |
| ENDERECO_03                                       |  21  | Saída   |
| SENSOR_DE_VAGA                                    |  27  | Entrada |
| SENSOR_DE_PASSAGEM_1                              |  22  | Entrada |
| SENSOR_DE_PASSAGEM_2                              |  11  | Entrada |
</center> 

<center>
<b>Tabela 3</b> - Pinout da GPIO da Raspberry Pi do <b>2º Andar</b>
</center>
<center> 

| Item                                              | GPIO | Direção |
|---------------------------------------------------|:----:|:-------:|
| ENDERECO_01                                       |   0  | Saída   |
| ENDERECO_02                                       |   5  | Saída   |
| ENDERECO_03                                       |   6  | Saída   |
| SENSOR_DE_VAGA                                    |  13  | Entrada |
| SENSOR_DE_PASSAGEM_1                              |  19  | Entrada |
| SENSOR_DE_PASSAGEM_2                              |  26  | Entrada |
</center> 

## 4. Integração RS485‑MODBUS

### 4.1 Endereçamento dos Dispositivos no Barramento

* **Câmera LPR da Entrada**: endereço **0x11**
* **Câmera LPR da Saída**: endereço **0x12**
* **Placar de Vagas**: endereço **0x20**

### 4.2 Mapa de Registros — Câmeras LPR (0x11 e 0x12)

**Holding Registers (0x03/0x10)**

| Offset | Tamanho | Descrição                              | Tipo/Formato                          |
| :----: | :-----: | -------------------------------------- | ------------------------------------- |
|    0   |    1    | **Status**                             | 0=Pronto, 1=Processando, 2=OK, 3=Erro |
|    1   |    1    | **Trigger Captura** (write‑1‑to‑start) | 0=Idle; escrever 1 dispara captura    |
|    2   |    4    | **Placa \[8 chars]**                   | 4 regs (2 bytes cada) — ASCII         |
|    6   |    1    | **Confiança (%)**                      | 0–100                                 |
|    7   |    1    | **Erro**                               | 0=none; outros conforme fabricante    |

**Fluxo típico (entrada/saída):**

1. Mestre escreve **1** em *Trigger Captura* (offset 1).
2. Faz *polling* em **Status** (offset 0) até **2=OK** ou **3=Erro** (timeout recomendado ≤ 2 s).
3. Se **OK**, lê **Placa** (offset 2, 4 regs) e **Confiança** (offset 6).
4. Zera *Trigger* (escrever 0 em offset 1, se necessário pelo dispositivo).

<!-- > **Conversão de Placa**: cada registrador de 16 bits carrega 2 chars ASCII em **big‑endian** (ex.: `0x4C50` → `LP`). Complementar com `\0` se a placa tiver < 8 chars. -->

### 4.3 Mapa de Registros — Placar de Vagas (0x20)

**Holding Registers (0x03/0x10)**

| Offset | Tamanho | Descrição                                  | Tipo |
| :----: | :-----: | ------------------------------------------ | ---- |
|    0   |    3    | **Vagas Livres Térreo (Vagas: Deficiente / Idoso+ / Comuns)**                    | u16 x 3 |
|    3   |    3    | **Vagas Livres Andar 1 (Vagas: Deficiente / Idoso+ / Comuns)**                   | u16 x 3 |
|    6   |    3    | **Vagas Livres Andar 2 (Vagas: Deficiente / Idoso+ / Comuns)**                   | u16 x 3 |
|    9   |    3    | **Vagas Livres Total (Vagas: Deficiente / Idoso+ / Comuns)**                     | u16 x 3 |
|   12   |    1    | **Flags** (bit0=lotado geral; bit1=blq 2º) | u16  |

**Observação**: o **Servidor Central** calcula os números e envia **Write Multiple Registers (0x10)** para offsets 0–11 periodicamente (p.ex. a cada 1 s) e sempre que houver mudança de estado.

### 4.4 Parâmetros e Retentativas

* **Serial**: 115200 bps, 8N1, RTS/DE controlado por driver (ou GPIO se necessário).
* **Timeout**: 200–500 ms por requisição.
* **Retries**: até 3, com backoff exponencial (p.ex. 100 ms, 250 ms, 500 ms).
* **Erros**: registrar exceções MODBUS (ILLEGAL FUNCTION, ILLEGAL DATA ADDRESS, CRC, timeout) no log.

## 5. Fluxos de Operação (com LPR)

### 5.1 Entrada

1. Sensor de presença da **cancela de entrada** ativa → Servidor do Andar Térreo e dispara **Trigger** na câmera **0x11**.
2. `Status` da câmera vai a *Processando* e retorna *OK* com **Placa** e **Confiança**.
3. Se *Confiança* ≥ limiar (p.ex. 70%), o **Servidor do Andar Térreo** envia ao **Servidor Central** o evento: `{placa, timestamp_entrada, origem: entrada}`.
4. O Central gera **Identificador** (ou reusa placa) e **abre a cancela**.
5. Sensor de passagem confirma e **fecha a cancela**.
6. Carro ocupa uma vaga; varredura de endereços registra a posição.

### 5.2 Saída

1. Sensor de presença na **cancela de saída** ativa → Servidor do 1º andar dispara **Trigger** na câmera **0x12**.
2. Obtém **Placa** (e confiança).
3. O **Servidor do Andar Térreo** envia ao **Servidor Central**: `{placa, timestamp_saida}`.
4. O Central busca `timestamp_entrada` correspondente, calcula **tempo de permanência** e **valor**.
5. O Central registra pagamento, comanda **abrir cancela de saída**; sensor de passagem confirma e cancela é **fechada**.
6. Atualiza contadores de vagas e **publica** no **placar (0x20)**.

### 5.3 Passagem entre Andares

* Sequência **1→2**: sobe para o 2º andar.
* Sequência **2→1**: desce para o 1º andar.
* Atualizar contagem por andar, sem alterar imediatamente a ocupação até a confirmação de vaga ocupada/desocupada.

## 6. Regras de Negócio

* **Preço**: **R\$ 0,15 por minuto** (arredondamento para cima em frações < 1 min é permitido se documentado).
* **Placa não lida/baixa confiança**: permitir entrada/saída com **ticket temporário** associado à vaga ou ID anônimo; reconciliar no Central via UI.
* **Carro sem correspondência de entrada** na saída: sinalizar alerta de auditoria (possível falha de leitura de entrada).
* **Lotado**: bloquear novas entradas; acender sinalização e refletir no **placar (flag bit0)**.
* **Bloqueio do 2º andar**: refletir no **placar (flag bit1)** e impedir novas alocações no 2º andar.

---

## 7. Interfaces e Protocolos Internos

### 7.1 Mensagens TCP/IP (sugestão JSON)

* **Evento de Entrada OK**

```json
{
  "tipo": "entrada_ok",
  "placa": "ABC1D23",
  "conf": 86,
  "ts": "2025-09-03T12:34:56Z",
  "andar": 1
}
```

* **Evento de Saída OK**

```json
{
  "tipo": "saida_ok",
  "placa": "ABC1D23",
  "conf": 83,
  "ts": "2025-09-03T15:10:00Z",
  "andar": 1
}
```

* **Atualização de Vagas** (Central → Placar via MODBUS e broadcast para UIs)

```json
{
  "tipo": "vaga_status",
  "livres_a1": 5,
  "livres_a2": 7,
  "livres_total": 12,
  "flags": {"lotado": false, "bloq2": false}
}
```
## 8. Implementação

### 8.1 Linguagens e Bibliotecas

* **Python**: `gpiozero`/`RPi.GPIO` (GPIO), `pymodbus` (MODBUS), `asyncio` (concorrência), `aiohttp`/`fastapi` (UI/API opcional).
* **C/C++**: `pigpio`/`wiringPi`/`BCM2835` (GPIO), `libmodbus` (MODBUS), `libevent`/`asio` (rede).

> Fornecer **Makefile** (C/C++) ou `requirements.txt` (Python) e instruções de execução.

### 8.2 Tarefas/Threads Sugeridas (Andar Térreo)

* `gpio_scan_task`: varredura de vagas (endereços 0–7).
* `gate_in_task`: máquina de estados da cancela de **entrada** + trigger LPR (0x11).
* `gate_out_task`: máquina de estados da cancela de **saída** + trigger LPR (0x12).
* `modbus_client`: fila de requisições (entrada/saída/placar) com **mutex** no mestre.

<!-- ### 8.3 Tratamento de Falhas

* **Timeout/CRC MODBUS**: repetir (máx. 3). Se persistir, **modo degradado**: operar cancela sem LPR, registrar ocorrência.
* **Câmera indisponível**: sinalizar na UI; permitir fluxo com ticket.
* **Placar indisponível**: continuar operação e re‑tentar atualização a cada 2 s. -->

## 9. Dashboards para Desenvolvimento

![](/figuras/Estacionamento_2.png)

Abaixo estão os links para cada Dashboard vinculado às respectivas placas Raspberry Pi

[Estacionamento - rasp41](https://tb.fse.lappis.rocks/dashboard/6edc7450-9a09-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8).  
[Estacionamento - rasp48](https://tb.fse.lappis.rocks/dashboard/882b4140-9b15-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8).  
[Estacionamento - rasp49](https://tb.fse.lappis.rocks/dashboard/af9c4d00-9b15-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8).  


1. Repositório (GitHub Classroom) com código, **README** de instalação/execução.
2. Vídeo (\~5 a 10 min) demonstrando: operação, fluxo LPR entrada/saída via MODBUS, atualização do placar e principais trechos de código.
3. Arquivos de configuração (ex.: `.env`) com parâmetros do bus: porta serial, baudrate, endereços.

<!-- ### 10.2 Critérios de Avaliação (12,0 pts + 0,5 extra)

| ITEM                        | DETALHE                                                      | VALOR |
| --------------------------- | ------------------------------------------------------------ | :---: |
| **Servidor Central**        |                                                              |       |
| Interface (Monitoramento)   | UI atualiza dados (andar, total, vagas, valor) em tempo real |  1,0  |
| Interface (Comandos)        | Fechar estacionamento / Bloquear 2º andar                    |  1,0  |
| Contagem de Vagas           | Correta por vaga/andar e lotação (geral e por andar)         |  1,0  |
| **Cobrança**                | Cálculo por minuto e recibo de saída                         |  1,0  |
| **Servidores Distribuídos** |                                                              |       |
| Vagas                       | Varredura e detecção de mudanças                             |  1,0  |
| Cancelas                    | Sequência correta entrada/saída                              |  1,0  |
| Passagem entre Andares      | Direção correta (1→2 sobe, 2→1 desce)                        |  1,0  |
| **MODBUS (Novo)**           |                                                              |       |
| Integração Câmera Entrada   | Trigger, polling, leitura de placa/confiança (0x11)          |  1,0  |
| Integração Câmera Saída     | Trigger, polling, leitura e envio p/ cobrança (0x12)         |  1,0  |
| Integração Placar           | Escrita periódica de vagas e flags (0x20)                    |  1,0  |
| Robustez Bus                | Tratamento de timeout/CRC, retries, logs                     |  1,0  |
| **Geral**                   |                                                              |       |
| Confiabilidade              | Reconexão TCP/IP automática, serviços independentes          |  0,5  |
| Qualidade do Código         | Nomes, modularização, desempenho                             |  1,5  |
| **Extra**                   | Usabilidade/qualidade acima da média                         |  0,5  |

--- -->

## 11. Referências Técnicas Sugeridas

* Documentação das bibliotecas GPIO e MODBUS escolhidas (p.ex. `gpiozero`, `RPi.GPIO`, `pigpio`, `libmodbus`, `pymodbus`).
