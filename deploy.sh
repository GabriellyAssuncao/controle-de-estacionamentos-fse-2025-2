#!/bin/bash

RASP_USER="laisbarbosa"
RASP_HOST="164.41.98.2"
RASP_PORT="15011"
RASP_DIR="estacionamento"

echo "════════════════════════════════════════════════════════════"
echo "   Deploy para rasp51"
echo "════════════════════════════════════════════════════════════"
echo ""

# Limpar build anterior
echo "🧹 Limpando build anterior..."
make clean

# Compilar em modo MOCK
echo "📦 Compilando em modo MOCK..."
make MOCK=1 all

if [ $? -ne 0 ]; then
    echo "❌ Erro na compilação!"
    exit 1
fi

echo "✓ Compilação concluída"
echo ""

# Criar scripts de execução na pasta build
echo "📝 Criando scripts de execução..."

# Script 1: run.sh (executar um servidor)
cat > build/run.sh << 'EOF'
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
EOF

# Script 2: run_all.sh (executar todos)
cat > build/run_all.sh << 'EOF'
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
EOF

# Script 3: stop_all.sh (parar todos)
cat > build/stop_all.sh << 'EOF'
#!/bin/bash
echo "Parando servidores..."
pkill -f servidor_central_mock
pkill -f servidor_terreo_mock
pkill -f servidor_andar1_mock
pkill -f servidor_andar2_mock
echo "✓ Servidores parados"
EOF

# Tornar scripts executáveis
chmod +x build/run.sh
chmod +x build/run_all.sh
chmod +x build/stop_all.sh

echo "✓ Scripts criados"
echo ""

# Transferir arquivos
echo "📤 Transferindo para rasp51..."
echo ""

scp -P $RASP_PORT -r config src Makefile README.md \
    $RASP_USER@$RASP_HOST:~/$RASP_DIR/

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Erro na transferência!"
    echo ""
    echo "Verifique se você consegue conectar:"
    echo "  ssh $RASP_USER@$RASP_HOST -p $RASP_PORT"
    exit 1
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "   ✓ Deploy concluído!"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Próximos passos:"
echo ""
echo "1. Conectar na rasp51:"
echo "   ssh $RASP_USER@$RASP_HOST -p $RASP_PORT"
echo ""
echo "2. Navegar para o projeto:"
echo "   cd $RASP_DIR/build"
echo ""
echo "3. Executar:"
echo "   ./run.sh              (um servidor por vez)"
echo "   ./run_all.sh          (todos juntos)"
echo "   tail -f logs/*.log    (ver logs)"
echo "   ./stop_all.sh         (parar tudo)"
echo ""