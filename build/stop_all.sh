#!/bin/bash
echo "Parando servidores..."
pkill -f servidor_central_mock
pkill -f servidor_terreo_mock
pkill -f servidor_andar1_mock
pkill -f servidor_andar2_mock
echo "✓ Servidores parados"
