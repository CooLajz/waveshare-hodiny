#!/bin/zsh

set -u

ROOT_DIR="${0:A:h}"

pause_before_close() {
  if [[ -t 0 ]]; then
    print
    print -n "Stisknutím libovolné klávesy zavřete okno..."
    read -k 1
    print
  fi
}

print "Waveshare Hodiny — sestavení a nahrání firmware"
print

if ! "$ROOT_DIR/build.sh"; then
  print -u2
  print -u2 "CHYBA: Firmware se nepodařilo sestavit."
  pause_before_close
  exit 1
fi

print
print "Sestavení je hotové. Nahrávám firmware do displeje..."
print

if ! "$ROOT_DIR/upload.sh"; then
  print -u2
  print -u2 "CHYBA: Firmware se nepodařilo nahrát."
  print -u2 "Zkontrolujte USB kabel a připojení právě jednoho displeje."
  pause_before_close
  exit 1
fi

print
print "HOTOVO: Nejnovější firmware byl úspěšně nahrán do displeje."
pause_before_close
