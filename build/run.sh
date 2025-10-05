#!/bin/bash
echo "════════════════════════════════════════════════════════════"
echo "   Sistema de Estacionamento - rasp51"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Escolha qual servidor executar:"
echo "1 - Servidor Central"
echo "2 - Servidor Térreo"
echo "3 - Servidor 1º Andar"
echo "4 - Servidor 2º Andar"
echo "0 - Sair"
echo ""
read -p "Opção: " opt

case $opt in
    1) ./servidor_central_mock ;;
    2) ./servidor_terreo_mock ;;
    3) ./servidor_andar1_mock ;;
    4) ./servidor_andar2_mock ;;
    0) echo "Saindo..." ;;
    *) echo "Opção inválida" ;;
esac
