#!/bin/bash
echo "Iniciando todos os servidores em background..."
mkdir -p logs

./servidor_central_mock > logs/central.log 2>&1 &
echo "✓ Servidor Central iniciado (PID: $!)"
sleep 1

./servidor_terreo_mock > logs/terreo.log 2>&1 &
echo "✓ Servidor Térreo iniciado (PID: $!)"
sleep 1

./servidor_andar1_mock > logs/andar1.log 2>&1 &
echo "✓ Servidor 1º Andar iniciado (PID: $!)"
sleep 1

./servidor_andar2_mock > logs/andar2.log 2>&1 &
echo "✓ Servidor 2º Andar iniciado (PID: $!)"

echo ""
echo "✓ Todos os servidores iniciados!"
echo ""
echo "Para ver logs:"
echo "  tail -f logs/central.log"
echo "  tail -f logs/terreo.log"
echo ""
echo "Para parar: ./stop_all.sh"
