#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for t in makerom bannertool; do
  command -v "$t" >/dev/null 2>&1 || { echo "ERRO: $t nao esta no PATH." >&2; exit 1; }
done
[ -f Impostor3DS.elf ] || { echo "ERRO: Impostor3DS.elf nao existe. Rode make primeiro." >&2; exit 1; }
mkdir -p build
cp silence.wav build/silence.wav
bannertool makesmdh -s "Impostor 3DS" -l "Blackout Edition - jogo nativo para Old 3DS" -p "Impostor 3DS" -i icon.png -o build/icon.smdh
bannertool makebanner -i banner.png -a build/silence.wav -o build/banner.bnr
rm -f Impostor3DS.cia
if ! makerom -f cia -o Impostor3DS.cia -target t -exefslogo -elf Impostor3DS.elf -rsf app.rsf -icon build/icon.smdh -banner build/banner.bnr; then
  echo "AVISO: makerom com app.rsf falhou; tentando o preset -desc app:4" >&2
  rm -f Impostor3DS.cia
  makerom -f cia -o Impostor3DS.cia -target t -exefslogo -elf Impostor3DS.elf -icon build/icon.smdh -banner build/banner.bnr -desc app:4
fi
[ -s Impostor3DS.cia ] || { echo "ERRO: CIA nao foi criado." >&2; exit 1; }
echo "CIA gerado: $(pwd)/Impostor3DS.cia"
