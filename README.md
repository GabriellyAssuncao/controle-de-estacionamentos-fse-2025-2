# Sistema de Controle de Estacionamento

Sistema distribuído para controle e monitoramento de estacionamento de 3 andares com câmeras LPR, sensores de vagas, cancelas automáticas e comunicação MODBUS/TCP-IP.

**Disciplina:** Fundamentos de Sistemas Embarcados (2025/2)  
**Versão:** 1.0

---

## 📋 Índice

- [Visão Geral](#-visão-geral)
- [Arquitetura](#-arquitetura)
- [Requisitos](#-requisitos)
- [Instalação](#-instalação)
- [Compilação](#-compilação)
- [Execução](#-execução)
- [Configuração](#-configuração)
- [Estrutura do Projeto](#-estrutura-do-projeto)
- [Funcionalidades](#-funcionalidades)
- [Troubleshooting](#-troubleshooting)

---

## 🎯 Visão Geral

O sistema controla um estacionamento com:

- **3 andares:** Térreo, 1º e 2º andar
- **20 vagas totais:** 4 no térreo, 8 no 1º andar, 8 no 2º andar
- **Tipos de vagas:** PNE, Idoso+ e Comuns
- **Cancelas automáticas** com sensores de abertura/fechamento
- **Câmeras LPR** (License Plate Recognition) via MODBUS para entrada e saída
- **Placar eletrônico** mostrando vagas disponíveis por andar
- **Cobrança** por tempo: R$ 0,15/minuto

### Componentes

1. **Servidor Central:** Consolida dados, calcula cobrança, interface de operação
2. **Servidor Térreo:** Controla cancelas, MODBUS (câmeras + placar), vagas térreo
3. **Servidor 1º Andar:** Monitora vagas, detecta passagem entre andares (↑↓)
4. **Servidor 2º Andar:** Monitora vagas, detecta saída para 1º andar (↓)

---

## 🏗️ Arquitetura

```
┌─────────────────────────────────────────────────────────────┐
│                    SERVIDOR CENTRAL                         │
│  - Consolidação de dados                                    │
│  - Cálculo de tarifas                                       │
│  - Interface de operação                                    │
│  - TCP Server (porta 8080)                                  │
└────────────────────────┬────────────────────────────────────┘
                         │ TCP/IP
        ┌────────────────┼────────────────┐
        │                │                │
┌───────▼────────┐ ┌────▼────────┐ ┌────▼────────┐
│  SERVIDOR      │ │  SERVIDOR   │ │  SERVIDOR   │
│  TÉRREO        │ │  1º ANDAR   │ │  2º ANDAR   │
│  ────────────  │ │  ──────────  │ │  ──────────  │
│  - Cancelas    │ │  - 8 vagas  │ │  - 8 vagas  │
│  - 4 vagas     │ │  - Passagem │ │  - Passagem │
│  -
```
