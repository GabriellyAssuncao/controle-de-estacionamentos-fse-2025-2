# Sistema de Controle de Estacionamento

Sistema completo de controle de estacionamento para Raspberry Pi com 3 andares usando bibliotecas padrão da indústria.

## 🏗️ Arquitetura

### **Hardware Suportado:**
- **Raspberry Pi** (testado em Pi 3/4)
- **GPIO** via `pigpio` (acesso hardware profissional)
- **RS485 MODBUS** via `libmodbus` (comunicação industrial)
- **TCP/IP** via `libevent` (alta performance assíncrona)

### **Bibliotecas Utilizadas:**
- ✅ **pigpio**: Controle GPIO robusto e preciso
- ✅ **libmodbus**: Cliente MODBUS RTU para equipamentos industriais  
- ✅ **libevent**: Framework assíncrono de rede de alta performance

## 🚀 Como Usar na Raspberry Pi

### **1. Compilar o Sistema**
```bash
# Compilar todos os componentes
make all

# Compilar apenas os testes
make tests
```

### **2. Executar Testes Individuais**

#### **Teste GPIO (requer sudo)**
```bash
# Teste básico
sudo make test-gpio

# Ou diretamente
sudo ./build/gpio_test -s    # Monitoramento contínuo
sudo ./build/gpio_test -t    # Teste básico
sudo ./build/gpio_test -a    # Teste completo
```

#### **Teste Parking Logic**
```bash
make test-parking
# ou
./build/parking_scan_test
```

#### **Teste Cancelas (requer sudo)**
```bash
sudo make test-gates
# ou  
sudo ./build/gate_test
```

#### **Teste MODBUS**
```bash
make test-modbus
# ou
./build/modbus_test /dev/ttyUSB0  # especificar porta serial
```

#### **Teste TCP**
```bash
make test-tcp
# ou
./build/tcp_test              # Modo interativo
./build/tcp_test -s           # Apenas servidor
./build/tcp_test -c 192.168.1.100  # Cliente conectando a IP
```

### **3. Script de Testes Automatizado**
```bash
# Menu interativo completo
./run_tests.sh

# Executar todos os testes
./run_tests.sh --all

# Testes específicos
./run_tests.sh --gpio
./run_tests.sh --parking
./run_tests.sh --gates
./run_tests.sh --modbus
./run_tests.sh --tcp

# Apenas compilar
./run_tests.sh --compile
```
- **Servidor Central**: Consolida ocupação, persiste eventos, calcula valores
- **Servidor Térreo**: Controla cancelas, comunica com MODBUS, lê sensores
- **Servidores 1º e 2º Andares**: Leem sensores, detectam passagens entre andares

## Compilação

### Dependências
```bash
sudo apt update
sudo apt install build-essential libmodbus-dev libjson-c-dev libpthread-stubs0-dev
```

### Compilar o projeto
```bash
make all
```

### Compilar módulos individualmente
```bash
make gpio_test      # Teste básico de GPIO
make servidor_central
make servidor_terreo
make servidor_andar1
make servidor_andar2
```

## Execução

### Testes individuais (recomendado para desenvolvimento)
```bash
# Teste básico de GPIO
./build/gpio_test

# Teste de varredura de vagas
./build/parking_scan_test

# Teste de cancelas
./build/gate_test

# Teste MODBUS
./build/modbus_test
```

### Execução dos servidores
```bash
# Terminal 1 - Servidor Central
./build/servidor_central

# Terminal 2 - Servidor Térreo
./build/servidor_terreo

# Terminal 3 - Servidor 1º Andar
./build/servidor_andar1

# Terminal 4 - Servidor 2º Andar
./build/servidor_andar2
```

## Configuração

Edite o arquivo `config/system_config.h` para ajustar:
- Portas TCP/IP
- Parâmetros MODBUS
- Configurações de GPIO
- Timeouts e tentativas

## Estrutura do Projeto

```
estacionamento/
├── src/
│   ├── common/           # Código comum a todos os módulos
│   ├── servidor_central/ # Servidor principal
│   ├── servidor_terreo/  # Servidor do térreo (com MODBUS)
│   ├── servidor_andar1/  # Servidor do 1º andar
│   └── servidor_andar2/  # Servidor do 2º andar
├── tests/               # Programas de teste
├── config/              # Arquivos de configuração
├── docs/               # Documentação
└── build/              # Binários compilados
```

## GPIO Mapping

### Andar Térreo
- ENDERECO_01: GPIO 17
- ENDERECO_02: GPIO 18
- ENDERECO_03: GPIO 23
- SENSOR_DE_VAGA: GPIO 8
- Cancelas: GPIO 1, 7, 12, 24, 25

### 1º Andar
- ENDERECO_01: GPIO 16
- ENDERECO_02: GPIO 20
- ENDERECO_03: GPIO 21
- SENSOR_DE_VAGA: GPIO 27
- Passagem: GPIO 11, 22

### 2º Andar
- ENDERECO_01: GPIO 0
- ENDERECO_02: GPIO 5
- ENDERECO_03: GPIO 6
- SENSOR_DE_VAGA: GPIO 13
- Passagem: GPIO 19, 26

## MODBUS Devices

- **Câmera LPR Entrada**: 0x11
- **Câmera LPR Saída**: 0x12
- **Placar de Vagas**: 0x20
- **Serial**: /dev/ttyUSB0, 115200 8N1